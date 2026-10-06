/*
 * lib-spfxd — reader/writer locks (reader-preferring).
 *
 * __lock: bits 0..30 hold the reader count, or RW_WRITER (all ones) when a
 * writer owns the lock; bit 31 (RW_WAIT) records that some thread sleeps.
 * The last unlocker clears RW_WAIT and wakes all sleepers, who then race
 * to reacquire.
 */
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "pthread_impl.h"

#define RW_WRITER 0x7fffffff
#define RW_WAIT   ((int)0x80000000)

int pthread_rwlockattr_init(pthread_rwlockattr_t *a) { a->__attr = 0; return 0; }
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *a) { return 0; }

int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *restrict a, int *restrict p)
{
	*p = (int)a->__attr;
	return 0;
}

int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *a, int p)
{
	if ((unsigned)p > 1) return EINVAL;
	a->__attr = (unsigned)p;
	return 0;
}

int pthread_rwlock_init(pthread_rwlock_t *restrict rw, const pthread_rwlockattr_t *restrict a)
{
	memset(rw, 0, sizeof *rw);
	if (a) rw->__shared = (int)a->__attr;
	return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *rw)
{
	return 0;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *rw)
{
	for (;;) {
		int v = rw->__lock, n = v & RW_WRITER;
		if (n == RW_WRITER) return EBUSY;
		if (n == RW_WRITER - 1) return EAGAIN;
		if (a_cas(&rw->__lock, v, v + 1) == v) return 0;
	}
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *rw)
{
	for (;;) {
		int v = rw->__lock;
		if (v & RW_WRITER) return EBUSY;
		if (a_cas(&rw->__lock, v, v | RW_WRITER) == v) return 0;
	}
}

static int sleep_on(pthread_rwlock_t *rw, int v, clockid_t clk, const struct timespec *at)
{
	if (!(v & RW_WAIT) && a_cas(&rw->__lock, v, v | RW_WAIT) != v) return 0;
	int r = __timedwait(&rw->__lock, v | RW_WAIT, clk, at, !rw->__shared, 1);
	return r == ETIMEDOUT || r == EINVAL ? r : 0;
}

static int rdlock(pthread_rwlock_t *rw, clockid_t clk, const struct timespec *at)
{
	int r;
	while ((r = pthread_rwlock_tryrdlock(rw)) == EBUSY) {
		int v = rw->__lock;
		if ((v & RW_WRITER) != RW_WRITER) continue;
		if ((r = sleep_on(rw, v, clk, at))) return r;
	}
	return r;
}

static int wrlock(pthread_rwlock_t *rw, clockid_t clk, const struct timespec *at)
{
	int spins = 100;
	while (pthread_rwlock_trywrlock(rw)) {
		int v = rw->__lock;
		if (!(v & RW_WRITER)) continue;
		if (spins-- > 0) { a_spin(); continue; }
		int r = sleep_on(rw, v, clk, at);
		if (r) return r;
	}
	return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rw) { return rdlock(rw, CLOCK_REALTIME, 0); }
int pthread_rwlock_wrlock(pthread_rwlock_t *rw) { return wrlock(rw, CLOCK_REALTIME, 0); }

int pthread_rwlock_timedrdlock(pthread_rwlock_t *restrict rw, const struct timespec *restrict at)
{
	return rdlock(rw, CLOCK_REALTIME, at);
}

int pthread_rwlock_timedwrlock(pthread_rwlock_t *restrict rw, const struct timespec *restrict at)
{
	return wrlock(rw, CLOCK_REALTIME, at);
}

int pthread_rwlock_clockrdlock(pthread_rwlock_t *restrict rw, clockid_t clk, const struct timespec *restrict at)
{
	if (clk != CLOCK_REALTIME && clk != CLOCK_MONOTONIC) return EINVAL;
	return rdlock(rw, clk, at);
}

int pthread_rwlock_clockwrlock(pthread_rwlock_t *restrict rw, clockid_t clk, const struct timespec *restrict at)
{
	if (clk != CLOCK_REALTIME && clk != CLOCK_MONOTONIC) return EINVAL;
	return wrlock(rw, clk, at);
}

int pthread_rwlock_unlock(pthread_rwlock_t *rw)
{
	for (;;) {
		int v = rw->__lock, n = v & RW_WRITER, nv;
		if (!n) return EPERM;
		nv = (n == RW_WRITER || n == 1) ? 0 : v - 1;
		if (a_cas(&rw->__lock, v, nv) == v) {
			if (!nv && (v & RW_WAIT)) __futex_wake(&rw->__lock, INT_MAX, !rw->__shared);
			return 0;
		}
	}
}
