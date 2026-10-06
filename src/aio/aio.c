/*
 * lib-spfxd — POSIX asynchronous I/O.
 *
 * Each request runs on its own short-lived thread, which performs the
 * pread/pwrite (or fsync) and then completes the request: the result is
 * stored, waiters in aio_suspend/lio_listio are woken through a global
 * completion counter (a futex), and the requested notification is sent
 * (a queued signal carrying sigev_value, or a call of the notification
 * function on the worker thread).  aio_fsync first waits until every
 * request submitted earlier for the same descriptor has completed.
 *
 * Requests on one descriptor may complete in any order (POSIX allows
 * this).  In-progress requests cannot be cancelled: aio_cancel reports
 * AIO_NOTCANCELED for them and AIO_ALLDONE when nothing is pending.
 */
#include <aio.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "atomic.h"
#include "lock.h"
#include "pthread_impl.h"
#include "syscall.h"

enum { OP_READ, OP_WRITE, OP_FSYNC, OP_DSYNC };

struct req {
	struct aiocb *cb;
	int op;
	unsigned long seq;
	struct req *next, *prev;
};

static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t done_cond = PTHREAD_COND_INITIALIZER;
static struct req *pending;
static unsigned long next_seq = 1;
static volatile int completions;           /* futex word for aio_suspend */

static int fcntl_is_append(int fd)
{
	int fl = fcntl(fd, F_GETFL);
	return fl >= 0 && (fl & O_APPEND);
}

static int fcntl_valid(int fd)
{
	return fcntl(fd, F_GETFD);
}

static int err_of(const struct aiocb *cb)
{
	return a_load((volatile int *)&cb->__err);
}

static void notify(struct sigevent *sev)
{
	if (sev->sigev_notify == SIGEV_SIGNAL) {
		siginfo_t si;
		memset(&si, 0, sizeof si);
		si.si_signo = sev->sigev_signo;
		si.si_code = SI_ASYNCIO;
		si.si_value = sev->sigev_value;
		si.si_pid = getpid();
		si.si_uid = getuid();
		__syscall(SYS_rt_sigqueueinfo, si.si_pid, si.si_signo, &si);
	} else if (sev->sigev_notify == SIGEV_THREAD && sev->sigev_notify_function) {
		sev->sigev_notify_function(sev->sigev_value);
	}
}

static int earlier_pending_on_fd(int fd, unsigned long seq)
{
	for (struct req *r = pending; r; r = r->next)
		if (r->cb->aio_fildes == fd && r->seq < seq && r->op != OP_FSYNC && r->op != OP_DSYNC)
			return 1;
	return 0;
}

static void *worker(void *arg)
{
	struct req *r = arg;
	struct aiocb *cb = r->cb;
	ssize_t ret;
	if (r->op == OP_FSYNC || r->op == OP_DSYNC) {
		pthread_mutex_lock(&lk);
		while (earlier_pending_on_fd(cb->aio_fildes, r->seq)) pthread_cond_wait(&done_cond, &lk);
		pthread_mutex_unlock(&lk);
	}
	switch (r->op) {
	case OP_READ:
		ret = pread(cb->aio_fildes, (void *)cb->aio_buf, cb->aio_nbytes, cb->aio_offset);
		break;
	case OP_WRITE:
		/* O_APPEND descriptors append regardless of the offset */
		ret = (fcntl_is_append(cb->aio_fildes))
		      ? write(cb->aio_fildes, (const void *)cb->aio_buf, cb->aio_nbytes)
		      : pwrite(cb->aio_fildes, (const void *)cb->aio_buf, cb->aio_nbytes, cb->aio_offset);
		break;
	case OP_DSYNC:
		ret = fdatasync(cb->aio_fildes);
		break;
	default:
		ret = fsync(cb->aio_fildes);
		break;
	}
	int err = ret < 0 ? errno : 0;
	struct sigevent sev = cb->aio_sigevent;

	pthread_mutex_lock(&lk);
	cb->__ret = ret;
	a_store(&cb->__err, err);
	if (r->prev) r->prev->next = r->next;
	else pending = r->next;
	if (r->next) r->next->prev = r->prev;
	a_inc(&completions);
	pthread_cond_broadcast(&done_cond);
	pthread_mutex_unlock(&lk);
	__futex_wake(&completions, -1, 1);
	free(r);

	notify(&sev);
	return 0;
}

static int submit(struct aiocb *cb, int op)
{
	if (op == OP_READ || op == OP_WRITE) {
		if (cb->aio_offset < 0 || cb->aio_nbytes > SSIZE_MAX) {
			errno = EINVAL;
			return -1;
		}
	}
	struct req *r = malloc(sizeof *r);
	if (!r) {
		errno = EAGAIN;
		return -1;
	}
	r->cb = cb;
	r->op = op;
	cb->__ret = 0;
	a_store(&cb->__err, EINPROGRESS);

	pthread_mutex_lock(&lk);
	r->seq = cb->__seq = next_seq++;
	r->prev = 0;
	r->next = pending;
	if (pending) pending->prev = r;
	pending = r;
	pthread_mutex_unlock(&lk);

	pthread_attr_t a;
	pthread_attr_init(&a);
	pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
	pthread_attr_setstacksize(&a, 128 * 1024);
	/* workers run with all signals blocked: notifications are explicit */
	sigset_t all, old;
	sigfillset(&all);
	pthread_sigmask(SIG_BLOCK, &all, &old);
	pthread_t t;
	int e = pthread_create(&t, &a, worker, r);
	pthread_sigmask(SIG_SETMASK, &old, 0);
	pthread_attr_destroy(&a);
	if (e) {
		pthread_mutex_lock(&lk);
		if (r->prev) r->prev->next = r->next;
		else pending = r->next;
		if (r->next) r->next->prev = r->prev;
		pthread_mutex_unlock(&lk);
		free(r);
		a_store(&cb->__err, EAGAIN);
		cb->__ret = -1;
		errno = EAGAIN;
		return -1;
	}
	return 0;
}

int aio_read(struct aiocb *cb) { return submit(cb, OP_READ); }
int aio_write(struct aiocb *cb) { return submit(cb, OP_WRITE); }

int aio_fsync(int op, struct aiocb *cb)
{
	if (op != O_SYNC && op != O_DSYNC) {
		errno = EINVAL;
		return -1;
	}
	return submit(cb, op == O_DSYNC ? OP_DSYNC : OP_FSYNC);
}

int aio_error(const struct aiocb *cb)
{
	return err_of(cb);
}

ssize_t aio_return(struct aiocb *cb)
{
	return cb->__ret;
}

int aio_cancel(int fd, struct aiocb *cb)
{
	int res = AIO_ALLDONE;
	if (cb && cb->aio_fildes != fd) {
		errno = EINVAL;
		return -1;
	}
	pthread_mutex_lock(&lk);
	for (struct req *r = pending; r; r = r->next)
		if (r->cb->aio_fildes == fd && (!cb || r->cb == cb)) res = AIO_NOTCANCELED;
	pthread_mutex_unlock(&lk);
	if (res == AIO_ALLDONE && fcntl_valid(fd) < 0) {
		errno = EBADF;
		return -1;
	}
	return res;
}

int aio_suspend(const struct aiocb *const list[], int n, const struct timespec *ts)
{
	struct timespec at;
	if (ts) {
		clock_gettime(CLOCK_MONOTONIC, &at);
		at.tv_sec += ts->tv_sec;
		at.tv_nsec += ts->tv_nsec;
		if (at.tv_nsec >= 1000000000) {
			at.tv_sec++;
			at.tv_nsec -= 1000000000;
		}
	}
	for (;;) {
		int seen = a_load(&completions);
		int any = 0;
		for (int i = 0; i < n; i++)
			if (list[i] && err_of(list[i]) != EINPROGRESS) any = 1;
		if (any) return 0;
		int r = __timedwait(&completions, seen, CLOCK_MONOTONIC, ts ? &at : 0, 1, 1);
		if (r == ETIMEDOUT) {
			errno = EAGAIN;
			return -1;
		}
		if (r == EINTR || r == ECANCELED) {
			errno = EINTR;
			return -1;
		}
	}
}

struct lio_wait {
	struct aiocb **list;
	int n;
	struct sigevent sev;
};

static void *lio_waiter(void *p)
{
	struct lio_wait *w = p;
	for (int i = 0; i < w->n; i++)
		while (w->list[i] && w->list[i]->aio_lio_opcode != LIO_NOP && aio_error(w->list[i]) == EINPROGRESS)
			aio_suspend((const struct aiocb *const *)&w->list[i], 1, 0);
	notify(&w->sev);
	free(w->list);
	free(w);
	return 0;
}

int lio_listio(int mode, struct aiocb *restrict const list[restrict], int n, struct sigevent *restrict sev)
{
	if ((mode != LIO_WAIT && mode != LIO_NOWAIT) || n < 0) {
		errno = EINVAL;
		return -1;
	}
	int failed = 0;
	for (int i = 0; i < n; i++) {
		struct aiocb *cb = list[i];
		if (!cb) continue;
		int r = 0;
		if (cb->aio_lio_opcode == LIO_READ) r = submit(cb, OP_READ);
		else if (cb->aio_lio_opcode == LIO_WRITE) r = submit(cb, OP_WRITE);
		if (r) failed = 1;
	}
	if (mode == LIO_WAIT) {
		for (int i = 0; i < n; i++) {
			struct aiocb *cb = list[i];
			if (!cb || cb->aio_lio_opcode == LIO_NOP) continue;
			while (aio_error(cb) == EINPROGRESS)
				if (aio_suspend((const struct aiocb *const *)&list[i], 1, 0) < 0 && errno == EINTR) {
					errno = EINTR;
					return -1;
				}
			if (aio_error(cb)) failed = 1;
		}
	} else if (sev && sev->sigev_notify != SIGEV_NONE) {
		struct lio_wait *w = malloc(sizeof *w);
		struct aiocb **copy = malloc((size_t)(n ? n : 1) * sizeof *copy);
		pthread_t t;
		pthread_attr_t a;
		if (!w || !copy) {
			free(w);
			free(copy);
			errno = EAGAIN;
			return -1;
		}
		for (int i = 0; i < n; i++) copy[i] = list[i];
		w->list = copy;
		w->n = n;
		w->sev = *sev;
		pthread_attr_init(&a);
		pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
		if (pthread_create(&t, &a, lio_waiter, w)) {
			free(copy);
			free(w);
			errno = EAGAIN;
			return -1;
		}
		pthread_attr_destroy(&a);
	}
	if (failed) {
		errno = EIO;
		return -1;
	}
	return 0;
}
