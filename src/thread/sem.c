/*
 * lib-spfxd — POSIX semaphores.
 *
 * Unnamed: __val is the count; __waiters lets sem_post skip the wake-up
 * system call when nobody sleeps.  Waiters sleep on the futex while the
 * count is zero.
 *
 * Named: a sem_t lives in a shared mapping of /dev/shm/sem.NAME.  Opening
 * the same name again in a process returns the same address (refcounted),
 * as POSIX requires.
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include "pthread_impl.h"

int sem_init(sem_t *s, int shared, unsigned value)
{
	if (value > SEM_VALUE_MAX) {
		errno = EINVAL;
		return -1;
	}
	memset(s, 0, sizeof *s);
	s->__val = (int)value;
	s->__shared = !!shared;
	return 0;
}

int sem_destroy(sem_t *s)
{
	return 0;
}

int sem_getvalue(sem_t *restrict s, int *restrict v)
{
	int x = s->__val;
	*v = x < 0 ? 0 : x;
	return 0;
}

int sem_post(sem_t *s)
{
	for (;;) {
		int v = s->__val;
		if (v == SEM_VALUE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
		if (a_cas(&s->__val, v, v + 1) == v) break;
	}
	if (a_load(&s->__waiters)) __futex_wake(&s->__val, 1, !s->__shared);
	return 0;
}

int sem_trywait(sem_t *s)
{
	int v;
	while ((v = s->__val) > 0)
		if (a_cas(&s->__val, v, v - 1) == v) return 0;
	errno = EAGAIN;
	return -1;
}

static void unwait(void *p)
{
	a_dec(p);
}

int sem_clockwait(sem_t *restrict s, clockid_t clk, const struct timespec *restrict at)
{
	pthread_testcancel();
	if (!sem_trywait(s)) return 0;
	for (int spins = 100; spins && s->__val <= 0; spins--) a_spin();
	while (sem_trywait(s)) {
		struct __spfxd_cleanup cb;
		a_inc(&s->__waiters);
		__spfxd_cleanup_push(&cb, unwait, (void *)&s->__waiters);
		int r = 0;
		if (s->__val <= 0) r = __timedwait(&s->__val, s->__val < 0 ? s->__val : 0, clk, at, !s->__shared, 1);
		__spfxd_cleanup_pop(&cb, 1);
		if (r && r != EINTR) {
			errno = r;
			return -1;
		}
		if (r == EINTR) {
			errno = EINTR;
			return -1;
		}
	}
	return 0;
}

int sem_timedwait(sem_t *restrict s, const struct timespec *restrict at)
{
	return sem_clockwait(s, CLOCK_REALTIME, at);
}

int sem_wait(sem_t *s)
{
	return sem_clockwait(s, CLOCK_REALTIME, 0);
}

/* ---- named semaphores ---- */

static struct named {
	struct named *next;
	dev_t dev;
	ino_t ino;
	sem_t *sem;
	int refs;
} *named_list;
static volatile int named_lock;

static int sem_path(const char *name, char *buf)
{
	while (*name == '/') name++;
	size_t n = strlen(name);
	if (!n || n > NAME_MAX - 4 || strchr(name, '/')) {
		errno = n ? ENAMETOOLONG : EINVAL;
		if (strchr(name, '/')) errno = EINVAL;
		return -1;
	}
	memcpy(buf, "/dev/shm/sem.", 13);
	memcpy(buf + 13, name, n + 1);
	return 0;
}

sem_t *sem_open(const char *name, int flags, ...)
{
	char path[NAME_MAX + 16];
	mode_t mode = 0;
	unsigned value = 0;
	struct stat st;
	int fd;

	if (sem_path(name, path)) return SEM_FAILED;
	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		value = va_arg(ap, unsigned);
		va_end(ap);
		if (value > SEM_VALUE_MAX) {
			errno = EINVAL;
			return SEM_FAILED;
		}
	}
	flags &= O_CREAT | O_EXCL;

	if (flags & O_CREAT) {
		/* Initialize in a temporary file, then link it into place so no
		 * other process can ever observe an uninitialized semaphore. */
		char tmp[NAME_MAX + 32];
		sem_t init;
		sem_init(&init, 1, value);
		for (int tries = 0;; tries++) {
			if (!(flags & O_EXCL)) {
				fd = open(path, O_RDWR | O_CLOEXEC);
				if (fd >= 0) break;
				if (errno != ENOENT) return SEM_FAILED;
			}
			snprintf(tmp, sizeof tmp, "/dev/shm/tmp-sem.%d.%d", getpid(), tries);
			int t = open(tmp, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, mode);
			if (t < 0) {
				if (errno == EEXIST && tries < 100) continue;
				return SEM_FAILED;
			}
			if (write(t, &init, sizeof init) != (ssize_t)sizeof init) {
				close(t);
				unlink(tmp);
				return SEM_FAILED;
			}
			int lr = link(tmp, path);
			unlink(tmp);
			if (!lr) {
				fd = t;
				break;
			}
			close(t);
			if (errno != EEXIST || (flags & O_EXCL) || tries > 100) return SEM_FAILED;
		}
	} else {
		fd = open(path, O_RDWR | O_CLOEXEC);
	}
	if (fd < 0) return SEM_FAILED;
	if (fstat(fd, &st) < 0 || st.st_size < (off_t)sizeof(sem_t)) {
		close(fd);
		errno = EINVAL;
		return SEM_FAILED;
	}

	__lock_always(&named_lock);
	for (struct named *n = named_list; n; n = n->next) {
		if (n->dev == st.st_dev && n->ino == st.st_ino) {
			n->refs++;
			__unlock_always(&named_lock);
			close(fd);
			return n->sem;
		}
	}
	sem_t *s = mmap(0, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	struct named *n = s == MAP_FAILED ? 0 : malloc(sizeof *n);
	if (!n) {
		if (s != MAP_FAILED) munmap(s, sizeof(sem_t));
		__unlock_always(&named_lock);
		return SEM_FAILED;
	}
	n->dev = st.st_dev;
	n->ino = st.st_ino;
	n->sem = s;
	n->refs = 1;
	n->next = named_list;
	named_list = n;
	__unlock_always(&named_lock);
	return s;
}

int sem_close(sem_t *s)
{
	__lock_always(&named_lock);
	for (struct named **pp = &named_list; *pp; pp = &(*pp)->next) {
		struct named *n = *pp;
		if (n->sem != s) continue;
		if (!--n->refs) {
			*pp = n->next;
			munmap(s, sizeof(sem_t));
			free(n);
		}
		__unlock_always(&named_lock);
		return 0;
	}
	__unlock_always(&named_lock);
	errno = EINVAL;
	return -1;
}

int sem_unlink(const char *name)
{
	char path[NAME_MAX + 16];
	if (sem_path(name, path)) return -1;
	return unlink(path);
}
