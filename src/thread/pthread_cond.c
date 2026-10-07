/*
 * lib-spfxd — condition variables.
 *
 * __seq is a wake-up sequence number.  A waiter samples it while still
 * holding the mutex, announces itself in __waiters, unlocks the mutex and
 * sleeps on the futex only while __seq still has the sampled value.  Any
 * signal or broadcast between the unlock and the sleep bumps __seq, so the
 * futex call returns immediately: no wake-up can be lost.  signal/broadcast
 * skip the system call entirely when nobody waits.
 *
 * Waiting is a cancellation point; a cleanup handler re-acquires the mutex
 * before cancellation handlers run, as POSIX requires.
 */
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "pthread_impl.h"

int pthread_condattr_init(pthread_condattr_t *a) { a->__attr = 0; return 0; }
int pthread_condattr_destroy(pthread_condattr_t *a) { return 0; }

int pthread_condattr_getclock(const pthread_condattr_t *restrict a, clockid_t *restrict c)
{
	*c = (clockid_t)(a->__attr & 0x7fffffff);
	return 0;
}

int pthread_condattr_setclock(pthread_condattr_t *a, clockid_t c)
{
	if (c != CLOCK_REALTIME && c != CLOCK_MONOTONIC) return EINVAL;
	a->__attr = (a->__attr & 0x80000000U) | (unsigned)c;
	return 0;
}

int pthread_condattr_getpshared(const pthread_condattr_t *restrict a, int *restrict p)
{
	*p = (int)(a->__attr >> 31);
	return 0;
}

int pthread_condattr_setpshared(pthread_condattr_t *a, int p)
{
	if ((unsigned)p > 1) return EINVAL;
	a->__attr = (a->__attr & 0x7fffffffU) | ((unsigned)p << 31);
	return 0;
}

int pthread_cond_init(pthread_cond_t *restrict c, const pthread_condattr_t *restrict a)
{
	memset(c, 0, sizeof *c);
	if (a) {
		c->__clock = (int)(a->__attr & 0x7fffffff);
		c->__shared = (int)(a->__attr >> 31);
	}
	return 0;
}

int pthread_cond_destroy(pthread_cond_t *c)
{
	return 0;
}

struct cond_wait_cleanup {
	pthread_cond_t *c;
	pthread_mutex_t *m;
};

static void unwait(void *p)
{
	struct cond_wait_cleanup *w = p;
	a_dec(&w->c->__waiters);
	__pthread_mutex_lock_internal(w->m);
}

hidden int __pthread_cond_timedwait_internal(pthread_cond_t *restrict c, pthread_mutex_t *restrict m,
	clockid_t clk, const struct timespec *restrict at)
{
	struct __spfxd_cleanup cb;
	struct cond_wait_cleanup w = { c, m };
	int r;

	if (at && (unsigned long)at->tv_nsec >= 1000000000UL) return EINVAL;
	pthread_testcancel();

	unsigned seq = c->__seq;
	a_inc(&c->__waiters);
	r = __pthread_mutex_unlock_internal(m);
	if (r) {
		a_dec(&c->__waiters);
		return r;
	}
	__spfxd_cleanup_push(&cb, unwait, &w);
	r = __timedwait((volatile int *)&c->__seq, (int)seq, clk, at, !c->__shared, 1);
	__spfxd_cleanup_pop(&cb, 1);
	return r == ETIMEDOUT ? ETIMEDOUT : 0;
}

int pthread_cond_wait(pthread_cond_t *restrict c, pthread_mutex_t *restrict m)
{
	return __pthread_cond_timedwait_internal(c, m, c->__clock, 0);
}

int pthread_cond_timedwait(pthread_cond_t *restrict c, pthread_mutex_t *restrict m,
	const struct timespec *restrict at)
{
	return __pthread_cond_timedwait_internal(c, m, c->__clock, at);
}

int pthread_cond_clockwait(pthread_cond_t *restrict c, pthread_mutex_t *restrict m,
	clockid_t clk, const struct timespec *restrict at)
{
	if (clk != CLOCK_REALTIME && clk != CLOCK_MONOTONIC) return EINVAL;
	return __pthread_cond_timedwait_internal(c, m, clk, at);
}

int pthread_cond_signal(pthread_cond_t *c)
{
	if (!a_load((volatile int *)&c->__waiters)) return 0;
	a_inc((volatile int *)&c->__seq);
	__futex_wake(&c->__seq, 1, !c->__shared);
	return 0;
}

int pthread_cond_broadcast(pthread_cond_t *c)
{
	if (!a_load((volatile int *)&c->__waiters)) return 0;
	a_inc((volatile int *)&c->__seq);
	__futex_wake(&c->__seq, INT_MAX, !c->__shared);
	return 0;
}
