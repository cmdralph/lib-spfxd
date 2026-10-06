/*
 * lib-spfxd — POSIX message queues.
 *
 * Names must begin with '/' (the kernel's mqueue filesystem wants them
 * without it).  mq_notify with SIGEV_THREAD uses the kernel's netlink
 * notification: a dedicated thread waits on a netlink socket for the
 * kernel's notification cookie and then runs the user function once, as
 * the notification is one-shot.
 */
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "syscall.h"

mqd_t mq_open(const char *name, int flags, ...)
{
	mode_t mode = 0;
	struct mq_attr *attr = 0;
	if (*name == '/') name++;
	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		attr = va_arg(ap, struct mq_attr *);
		va_end(ap);
	}
	return (mqd_t)__sysret(SYS_mq_open, name, flags, mode, attr);
}

int mq_close(mqd_t d)
{
	return (int)__sysret(SYS_close, d);
}

int mq_unlink(const char *name)
{
	if (*name == '/') name++;
	int r = (int)__syscall(SYS_mq_unlink, name);
	if (r < 0) {
		if (r == -EPERM) r = -EACCES;
		errno = -r;
		return -1;
	}
	return 0;
}

int mq_getattr(mqd_t d, struct mq_attr *a)
{
	return mq_setattr(d, 0, a);
}

int mq_setattr(mqd_t d, const struct mq_attr *restrict n, struct mq_attr *restrict o)
{
	return (int)__sysret(SYS_mq_getsetattr, d, n, o);
}

int mq_timedsend(mqd_t d, const char *msg, size_t len, unsigned prio, const struct timespec *ts)
{
	return (int)__sysret_cp(SYS_mq_timedsend, d, msg, len, prio, ts);
}

int mq_send(mqd_t d, const char *msg, size_t len, unsigned prio)
{
	return mq_timedsend(d, msg, len, prio, 0);
}

ssize_t mq_timedreceive(mqd_t d, char *restrict msg, size_t len, unsigned *restrict prio,
                        const struct timespec *restrict ts)
{
	return __sysret_cp(SYS_mq_timedreceive, d, msg, len, prio, ts);
}

ssize_t mq_receive(mqd_t d, char *msg, size_t len, unsigned *prio)
{
	return mq_timedreceive(d, msg, len, prio, 0);
}

struct notify_args {
	int sock;
	void (*fn)(union sigval);
	union sigval val;
	pthread_barrier_t barrier;
};

static void *notify_thread(void *p)
{
	struct notify_args *a = p;
	int sock = a->sock;
	void (*fn)(union sigval) = a->fn;
	union sigval val = a->val;
	pthread_barrier_wait(&a->barrier);       /* after this, *a is gone */
	char cookie[32];
	ssize_t n = recv(sock, cookie, sizeof cookie, MSG_WAITALL | MSG_NOSIGNAL);
	close(sock);
	if (n == (ssize_t)sizeof cookie && cookie[31] == 1 /* NOTIFY_WOKENUP */) fn(val);
	return 0;
}

int mq_notify(mqd_t d, const struct sigevent *sev)
{
	if (!sev || sev->sigev_notify != SIGEV_THREAD)
		return (int)__sysret(SYS_mq_notify, d, sev);

	struct notify_args a;
	a.sock = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, 0);
	if (a.sock < 0) return -1;
	a.fn = sev->sigev_notify_function;
	a.val = sev->sigev_value;
	pthread_attr_t attr;
	if (sev->sigev_notify_attributes) attr = *sev->sigev_notify_attributes;
	else pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_barrier_init(&a.barrier, 0, 2);
	pthread_t t;
	int e = pthread_create(&t, &attr, notify_thread, &a);
	if (e) {
		close(a.sock);
		errno = e;
		return -1;
	}
	pthread_barrier_wait(&a.barrier);
	pthread_barrier_destroy(&a.barrier);

	/* the kernel delivers a 32-byte cookie to the netlink socket */
	static char cookie[32];
	struct sigevent kev;
	memset(&kev, 0, sizeof kev);
	kev.sigev_notify = SIGEV_THREAD;
	kev.sigev_signo = a.sock;
	kev.sigev_value.sival_ptr = cookie;
	long r = __syscall(SYS_mq_notify, d, &kev);
	if (r < 0) {
		shutdown(a.sock, SHUT_RDWR);           /* wake the thread so it exits */
		errno = (int)-r;
		return -1;
	}
	return 0;
}
