/*
 * lib-spfxd — mutexes.
 *
 * __lock: 0 unlocked, 1 locked, 2 locked with (possible) waiters.
 * Normal mutexes use only __lock (no owner bookkeeping on the fast path).
 * Recursive and error-checking mutexes also record the owner's tid.
 * Process-shared mutexes use shared (non-private) futex operations.
 *
 * Robust mutexes and the priority-inheritance/protection protocols are not
 * provided; the attribute setters report ENOTSUP for them.
 */
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "pthread_impl.h"

#define MT_TYPE_MASK 3
#define MT_SHARED 128

int pthread_mutexattr_init(pthread_mutexattr_t *a) { a->__attr = 0; return 0; }
int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { return 0; }

int pthread_mutexattr_gettype(const pthread_mutexattr_t *restrict a, int *restrict t)
{
	*t = (int)(a->__attr & MT_TYPE_MASK);
	return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *a, int t)
{
	if ((unsigned)t > 2) return EINVAL;
	a->__attr = (a->__attr & ~MT_TYPE_MASK) | (unsigned)t;
	return 0;
}

int pthread_mutexattr_getpshared(const pthread_mutexattr_t *restrict a, int *restrict p)
{
	*p = !!(a->__attr & MT_SHARED);
	return 0;
}

int pthread_mutexattr_setpshared(pthread_mutexattr_t *a, int p)
{
	if ((unsigned)p > 1) return EINVAL;
	a->__attr = (a->__attr & ~MT_SHARED) | (p ? MT_SHARED : 0);
	return 0;
}

int pthread_mutexattr_getrobust(const pthread_mutexattr_t *restrict a, int *restrict r)
{
	*r = PTHREAD_MUTEX_STALLED;
	return 0;
}

int pthread_mutexattr_setrobust(pthread_mutexattr_t *a, int r)
{
	if (r == PTHREAD_MUTEX_STALLED) return 0;
	return r == PTHREAD_MUTEX_ROBUST ? ENOTSUP : EINVAL;
}

int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *restrict a, int *restrict p)
{
	*p = PTHREAD_PRIO_NONE;
	return 0;
}

int pthread_mutexattr_setprotocol(pthread_mutexattr_t *a, int p)
{
	if (p == PTHREAD_PRIO_NONE) return 0;
	return (p == PTHREAD_PRIO_INHERIT || p == PTHREAD_PRIO_PROTECT) ? ENOTSUP : EINVAL;
}

int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *restrict a, int *restrict c)
{
	return EINVAL;
}

int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *a, int c)
{
	return EINVAL;
}

int pthread_mutex_init(pthread_mutex_t *restrict m, const pthread_mutexattr_t *restrict a)
{
	memset(m, 0, sizeof *m);
	if (a) m->__type = (int)a->__attr;
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *m)
{
	return m->__lock ? EBUSY : 0;
}

static __inline int priv(const pthread_mutex_t *m)
{
	return !(m->__type & MT_SHARED);
}

/* Contended path.  There is deliberately no spinning before sleeping:
 * measured on current x86 (where pause costs ~100+ cycles) and under
 * virtualization, spinning only delayed the hand-off; the futex wait is
 * entered at once, as glibc's default mutex does. */
static int lock_word(pthread_mutex_t *m, clockid_t clk, const struct timespec *at)
{
	if (!a_cas(&m->__lock, 0, 1)) return 0;
	while (a_swap(&m->__lock, 2)) {
		int r = __timedwait(&m->__lock, 2, clk, at, priv(m), 0);
		if (r == ETIMEDOUT || r == EINVAL) return r;
	}
	return 0;
}

hidden int __pthread_mutex_timedlock_internal(pthread_mutex_t *m, clockid_t clk, const struct timespec *at)
{
	int type = m->__type & MT_TYPE_MASK;
	if (type == PTHREAD_MUTEX_NORMAL) {
		if (!a_cas(&m->__lock, 0, 1)) return 0;
		return lock_word(m, clk, at);
	}
	int tid = __self()->tid;
	if (m->__owner == tid) {
		if (type == PTHREAD_MUTEX_ERRORCHECK) return EDEADLK;
		if (m->__count == INT_MAX) return EAGAIN;
		m->__count++;
		return 0;
	}
	int r = a_cas(&m->__lock, 0, 1) ? lock_word(m, clk, at) : 0;
	if (r) return r;
	m->__owner = tid;
	m->__count = 0;
	return 0;
}

hidden int __pthread_mutex_lock_internal(pthread_mutex_t *m)
{
	return __pthread_mutex_timedlock_internal(m, CLOCK_REALTIME, 0);
}

int pthread_mutex_lock(pthread_mutex_t *m)
{
	if ((m->__type & MT_TYPE_MASK) == PTHREAD_MUTEX_NORMAL && !a_cas(&m->__lock, 0, 1))
		return 0;
	return __pthread_mutex_timedlock_internal(m, CLOCK_REALTIME, 0);
}

int pthread_mutex_timedlock(pthread_mutex_t *restrict m, const struct timespec *restrict at)
{
	return __pthread_mutex_timedlock_internal(m, CLOCK_REALTIME, at);
}

int pthread_mutex_clocklock(pthread_mutex_t *restrict m, clockid_t clk, const struct timespec *restrict at)
{
	if (clk != CLOCK_REALTIME && clk != CLOCK_MONOTONIC) return EINVAL;
	return __pthread_mutex_timedlock_internal(m, clk, at);
}

int pthread_mutex_trylock(pthread_mutex_t *m)
{
	int type = m->__type & MT_TYPE_MASK;
	if (type == PTHREAD_MUTEX_NORMAL) return a_cas(&m->__lock, 0, 1) ? EBUSY : 0;
	int tid = __self()->tid;
	if (m->__owner == tid) {
		if (type == PTHREAD_MUTEX_ERRORCHECK) return EBUSY;
		if (m->__count == INT_MAX) return EAGAIN;
		m->__count++;
		return 0;
	}
	if (a_cas(&m->__lock, 0, 1)) return EBUSY;
	m->__owner = tid;
	m->__count = 0;
	return 0;
}

hidden int __pthread_mutex_unlock_internal(pthread_mutex_t *m)
{
	int type = m->__type & MT_TYPE_MASK;
	if (type != PTHREAD_MUTEX_NORMAL) {
		if (m->__owner != __self()->tid) return EPERM;
		if (m->__count) {
			m->__count--;
			return 0;
		}
		m->__owner = 0;
	}
	if (a_swap(&m->__lock, 0) == 2) __futex_wake(&m->__lock, 1, priv(m));
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *m)
{
	/* normal mutexes: one exchange, a wake only if someone sleeps */
	if ((m->__type & MT_TYPE_MASK) == PTHREAD_MUTEX_NORMAL) {
		if (a_swap(&m->__lock, 0) == 2) __futex_wake(&m->__lock, 1, priv(m));
		return 0;
	}
	return __pthread_mutex_unlock_internal(m);
}

int pthread_mutex_consistent(pthread_mutex_t *m)
{
	return EINVAL;
}

int pthread_mutex_getprioceiling(const pthread_mutex_t *restrict m, int *restrict c)
{
	return EINVAL;
}

int pthread_mutex_setprioceiling(pthread_mutex_t *restrict m, int c, int *restrict old)
{
	return EINVAL;
}
