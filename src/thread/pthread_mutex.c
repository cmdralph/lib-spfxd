/*
 * lib-spfxd — mutexes.
 *
 * Plain mutexes (no robustness, PTHREAD_PRIO_NONE or _PROTECT)
 *   __lock: 0 unlocked, 1 locked, 2 locked with (possible) waiters.
 *   Normal mutexes use only __lock (no owner bookkeeping on the fast
 *   path); recursive and error-checking ones also record the owner's tid
 *   in __owner.
 *
 * Robust and priority-inheritance mutexes
 *   __lock is in the kernel's format: the owner's tid, FUTEX_WAITERS and
 *   FUTEX_OWNER_DIED.
 *   - Robust mutexes are linked through __next/__prev into the owning
 *     thread's robust list (struct pthread .robust, registered with
 *     set_robust_list), so that the kernel can mark them FUTEX_OWNER_DIED
 *     and wake a waiter when the owner dies.  `pending` covers the window
 *     in which a mutex is being acquired or released.  A normal thread
 *     exit does the same marking in user space first
 *     (__pthread_robust_exit), because a detached thread's TCB is unmapped
 *     before its final exit system call.
 *   - The next owner gets EOWNERDEAD and the mutex is inconsistent
 *     (MT_INCONSISTENT) until pthread_mutex_consistent(); unlocking it
 *     while inconsistent makes it permanently ENOTRECOVERABLE
 *     (MT_NOTRECOVERABLE) and wakes every waiter.
 *   - Priority inheritance uses FUTEX_LOCK_PI / FUTEX_UNLOCK_PI: the
 *     kernel boosts the owner while higher-priority threads wait.
 *     FUTEX_LOCK_PI2 serves CLOCK_MONOTONIC deadlines.
 *
 * Priority protection (PTHREAD_PRIO_PROTECT)
 *   The kernel has no ceiling protocol; it is done here.  Each thread
 *   counts the ceilings of the protected mutexes it holds (pp_count); a
 *   thread with a real-time policy (SCHED_FIFO/SCHED_RR) runs at
 *   max(its base priority, highest held ceiling).  Threads with other
 *   policies have no static priority to raise, so for them a ceiling has
 *   no effect.  Locking fails with EINVAL when a real-time caller's base
 *   priority is above the ceiling.
 *
 * Process-shared mutexes use shared (non-private) futex operations.
 */
#include <errno.h>
#include <limits.h>
#include <sched.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include "pthread_impl.h"

/* __type (and the attribute word): kind and protocol */
#define MT_TYPE_MASK 3
#define MT_ROBUST 4
#define MT_PI 8
#define MT_PP 16
#define MT_KIND (MT_TYPE_MASK | MT_ROBUST | MT_PI | MT_PP)
#define MT_SHARED 128
#define ATTR_CEIL_SHIFT 16          /* attribute only: priority ceiling */
/* __type: run-time state of robust mutexes */
#define MT_INCONSISTENT (1 << 24)
#define MT_NOTRECOVERABLE (1 << 25)

#define FUTEX_LOCK_PI 6
#define FUTEX_UNLOCK_PI 7
#define FUTEX_TRYLOCK_PI 8
#define FUTEX_LOCK_PI2 13
#define FUTEX_WAITERS ((int)0x80000000)
#define FUTEX_OWNER_DIED 0x40000000
#define FUTEX_TID_MASK 0x3fffffff

#define PRIO_MAX_RT 99

_Static_assert(offsetof(pthread_mutex_t, __prev) == offsetof(pthread_mutex_t, __next) + sizeof(void *),
	"robust list node layout");

/* ------------------------------------------------------- attributes */

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
	*r = a->__attr & MT_ROBUST ? PTHREAD_MUTEX_ROBUST : PTHREAD_MUTEX_STALLED;
	return 0;
}

int pthread_mutexattr_setrobust(pthread_mutexattr_t *a, int r)
{
	if ((unsigned)r > 1) return EINVAL;
	a->__attr = (a->__attr & ~MT_ROBUST) | (r ? MT_ROBUST : 0);
	return 0;
}

int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *restrict a, int *restrict p)
{
	*p = a->__attr & MT_PI ? PTHREAD_PRIO_INHERIT : a->__attr & MT_PP ? PTHREAD_PRIO_PROTECT : PTHREAD_PRIO_NONE;
	return 0;
}

int pthread_mutexattr_setprotocol(pthread_mutexattr_t *a, int p)
{
	unsigned f;
	switch (p) {
	case PTHREAD_PRIO_NONE: f = 0; break;
	case PTHREAD_PRIO_INHERIT: f = MT_PI; break;
	case PTHREAD_PRIO_PROTECT: f = MT_PP; break;
	default: return EINVAL;
	}
	a->__attr = (a->__attr & ~(unsigned)(MT_PI | MT_PP)) | f;
	return 0;
}

int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *restrict a, int *restrict c)
{
	*c = (int)((a->__attr >> ATTR_CEIL_SHIFT) & 0xff);
	return 0;
}

int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *a, int c)
{
	if (c < 1 || c > PRIO_MAX_RT) return EINVAL;
	a->__attr = (a->__attr & ~(0xffu << ATTR_CEIL_SHIFT)) | ((unsigned)c << ATTR_CEIL_SHIFT);
	return 0;
}

/* ------------------------------------------------------- init/destroy */

int pthread_mutex_init(pthread_mutex_t *restrict m, const pthread_mutexattr_t *restrict a)
{
	memset(m, 0, sizeof *m);
	if (a) {
		m->__type = (int)(a->__attr & (MT_KIND | MT_SHARED));
		m->__prio = (int)((a->__attr >> ATTR_CEIL_SHIFT) & 0xff);
	}
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

static __inline int tid_owned(const pthread_mutex_t *m)
{
	return m->__type & (MT_ROBUST | MT_PI);
}

/* ------------------------------------------------- priority protection */

static int is_rt(int policy)
{
	policy &= ~SCHED_RESET_ON_FORK;
	return policy == SCHED_FIFO || policy == SCHED_RR;
}

static int pp_highest(const struct pthread *t)
{
	for (int c = PRIO_MAX_RT; c > 0; c--)
		if (t->pp_count[c]) return c;
	return 0;
}

/* Run thread t (the caller, or t whose state the caller owns) at
 * max(base, highest held ceiling).  cur/policy: its current values. */
static int pp_apply(struct pthread *t, int policy, int cur)
{
	if (!is_rt(policy)) {
		t->pp_boosted = 0;
		return 0;
	}
	int held = pp_highest(t);
	if (!t->pp_boosted) {
		if (!held) return 0;
		t->pp_base = cur;
		t->pp_boosted = 1;
	}
	int target = held > t->pp_base ? held : t->pp_base;
	if (!held) t->pp_boosted = 0;
	if (target == cur) return 0;
	struct sched_param sp;
	memset(&sp, 0, sizeof sp);
	sp.sched_priority = target;
	return (int)-__syscall(SYS_sched_setparam, t->tid, &sp);
}

static int pp_query(int *policy, int *cur)
{
	struct sched_param sp;
	long r = __syscall(SYS_sched_getscheduler, 0);
	if (r < 0) return (int)-r;
	*policy = (int)r;
	r = __syscall(SYS_sched_getparam, 0, &sp);
	if (r < 0) return (int)-r;
	*cur = sp.sched_priority;
	return 0;
}

static int pp_enter(struct pthread *self, int ceil)
{
	int policy = 0, cur = 0, r;
	if (ceil < 0 || ceil > PRIO_MAX_RT) return EINVAL;
	if ((r = pp_query(&policy, &cur))) return r;
	if (is_rt(policy) && (self->pp_boosted ? self->pp_base : cur) > ceil) return EINVAL;
	self->pp_count[ceil]++;
	if ((r = pp_apply(self, policy, cur))) {
		self->pp_count[ceil]--;
		return r;
	}
	return 0;
}

static void pp_leave(struct pthread *self, int ceil)
{
	int policy = 0, cur = 0;
	if (ceil < 0 || ceil > PRIO_MAX_RT || !self->pp_count[ceil]) return;
	self->pp_count[ceil]--;
	if (!pp_query(&policy, &cur)) pp_apply(self, policy, cur);
}

/* pthread_setschedparam/setschedprio on a thread that holds ceilings:
 * the new priority becomes its base and the boost is reapplied. */
hidden void __pthread_pp_rebase(struct pthread *t, int policy, int prio)
{
	if (!t->pp_boosted) return;
	t->pp_base = prio;
	pp_apply(t, policy, -1);
}

/* ---------------------------------------------------- robust list */

static void robust_register(struct pthread *self)
{
	if (self->robust_registered) return;
	self->robust.head = (void *)&self->robust.head;
	self->robust.futex_offset = (long)offsetof(pthread_mutex_t, __lock) - (long)offsetof(pthread_mutex_t, __next);
	self->robust.pending = 0;
	/* may fail (ENOSYS under some emulators); thread exits are still
	 * handled in user space, only process death is not */
	__syscall(SYS_set_robust_list, &self->robust, 3 * sizeof(long));
	self->robust_registered = 1;
}

static void robust_add(struct pthread *self, pthread_mutex_t *m)
{
	void *head = (void *)&self->robust.head;
	void *first = self->robust.head;
	m->__next = first;
	m->__prev = head;
	if (first != head) ((void *volatile *)first)[1] = (void *)&m->__next;
	self->robust.head = (void *)&m->__next;
}

static void robust_del(struct pthread *self, pthread_mutex_t *m)
{
	void *next = m->__next;
	void *volatile *prev = m->__prev;
	*prev = next;
	if (next != (void *)&self->robust.head) ((void *volatile *)next)[1] = (void *)prev;
	m->__next = m->__prev = 0;
}

/* The owner `tid` is gone: hand the mutex on with FUTEX_OWNER_DIED, as
 * the kernel does for a dead thread.  A PI mutex with waiters keeps the
 * tid: the kernel passes it to the top waiter when the thread exits. */
static void mark_owner_dead(pthread_mutex_t *m, int tid)
{
	for (;;) {
		int v = m->__lock;
		if ((v & FUTEX_TID_MASK) != tid) return;
		int pi_waiters = (m->__type & MT_PI) && (v & FUTEX_WAITERS);
		int nv = pi_waiters ? v | FUTEX_OWNER_DIED : (v & FUTEX_WAITERS) | FUTEX_OWNER_DIED;
		if (a_cas(&m->__lock, v, nv) == v) {
			if (!pi_waiters && (v & FUTEX_WAITERS)) __futex_wake(&m->__lock, 1, priv(m));
			return;
		}
	}
}

hidden void __pthread_robust_exit(struct pthread *self)
{
	if (!self->robust_registered) return;
	void *head = (void *)&self->robust.head;
	void *n = self->robust.head;
	self->robust.head = head;
	self->robust.pending = 0;
	while (n && n != head) {
		pthread_mutex_t *m = (void *)((char *)n - offsetof(pthread_mutex_t, __next));
		n = m->__next;                  /* before the next owner relinks it */
		m->__next = m->__prev = 0;
		mark_owner_dead(m, self->tid);
	}
}

/* ------------------------------------------------- lock word protocols */

/* Plain: contended path.  There is deliberately no spinning before
 * sleeping: measured on current x86 (where pause costs ~100+ cycles) and
 * under virtualization, spinning only delayed the hand-off; the futex
 * wait is entered at once, as glibc's default mutex does. */
static int lock_word(pthread_mutex_t *m, clockid_t clk, const struct timespec *at)
{
	if (!a_cas(&m->__lock, 0, 1)) return 0;
	while (a_swap(&m->__lock, 2)) {
		int r = __timedwait(&m->__lock, 2, clk, at, priv(m), 0);
		if (r == ETIMEDOUT || r == EINVAL) return r;
	}
	return 0;
}

/* Robust (not PI): tid-based word, waiters sleep on FUTEX_WAITERS. */
static int robust_lock_word(pthread_mutex_t *m, int tid, clockid_t clk, const struct timespec *at, int try)
{
	int waited = 0;
	for (;;) {
		int v = m->__lock;
		if (!(v & FUTEX_TID_MASK)) {
			/* free, possibly after its owner died; once we have slept
			 * there may be other sleepers, so keep FUTEX_WAITERS */
			int nv = tid | (v & FUTEX_WAITERS) | (waited ? FUTEX_WAITERS : 0);
			if (a_cas(&m->__lock, v, nv) != v) continue;
			return v & FUTEX_OWNER_DIED ? EOWNERDEAD : 0;
		}
		if (try) return EBUSY;
		if (!(v & FUTEX_WAITERS)) {
			if (a_cas(&m->__lock, v, v | FUTEX_WAITERS) != v) continue;
			v |= FUTEX_WAITERS;
		}
		waited = 1;
		int r = __timedwait(&m->__lock, v, clk, at, priv(m), 0);
		if (r == ETIMEDOUT || r == EINVAL) return r;
		if (m->__type & MT_NOTRECOVERABLE) return ENOTRECOVERABLE;
	}
}

static long futex_pi(pthread_mutex_t *m, int op, const struct timespec *ts)
{
	return __syscall(SYS_futex, &m->__lock, op | (priv(m) ? FUTEX_PRIVATE : 0), 0, ts);
}

/* PI: the kernel queues waiters by priority and boosts the owner. */
static int pi_lock_word(pthread_mutex_t *m, int tid, clockid_t clk, const struct timespec *at, int try)
{
	for (;;) {
		int v = m->__lock;
		if (v == 0 || v == FUTEX_OWNER_DIED) {
			/* free and without kernel state: take it in user space */
			if (a_cas(&m->__lock, v, tid) != v) continue;
			return v ? EOWNERDEAD : 0;
		}
		long r;
		if (try) {
			/* only the kernel can take over a dead owner's word with
			 * waiters; "held" comes back as EWOULDBLOCK (== EAGAIN) */
			if (v & FUTEX_TID_MASK && !(v & FUTEX_OWNER_DIED)) return EBUSY;
			r = futex_pi(m, FUTEX_TRYLOCK_PI, 0);
			if (r == -EAGAIN || r == -EBUSY || r == -EDEADLK) return EBUSY;
		} else if (clk == CLOCK_MONOTONIC && at) {
			r = futex_pi(m, FUTEX_LOCK_PI2, at);
			if (r == -ENOSYS || r == -EINVAL) {
				/* kernel before 5.14: convert to a CLOCK_REALTIME deadline */
				struct timespec now_m, now_r, rt;
				clock_gettime(CLOCK_MONOTONIC, &now_m);
				clock_gettime(CLOCK_REALTIME, &now_r);
				rt.tv_sec = now_r.tv_sec + (at->tv_sec - now_m.tv_sec);
				rt.tv_nsec = now_r.tv_nsec + (at->tv_nsec - now_m.tv_nsec);
				while (rt.tv_nsec < 0) { rt.tv_nsec += 1000000000; rt.tv_sec--; }
				while (rt.tv_nsec >= 1000000000) { rt.tv_nsec -= 1000000000; rt.tv_sec++; }
				r = futex_pi(m, FUTEX_LOCK_PI, &rt);
			}
		} else {
			r = futex_pi(m, FUTEX_LOCK_PI, at);   /* absolute CLOCK_REALTIME */
		}
		switch (r) {
		case 0:
			v = m->__lock;
			if (v & FUTEX_OWNER_DIED) {
				/* we own it: drop the bit (the kernel may add WAITERS) */
				while (a_cas(&m->__lock, v, v & ~FUTEX_OWNER_DIED) != v) v = m->__lock;
				return EOWNERDEAD;
			}
			return 0;
		case -EAGAIN:                   /* owner exiting or word changed */
		case -EINTR:
		case -ESRCH:
			continue;
		default:
			return (int)-r;          /* ETIMEDOUT, EINVAL, EDEADLK, ENOSYS, ... */
		}
	}
}

static void pi_unlock_word(pthread_mutex_t *m, int tid)
{
	if (a_cas(&m->__lock, tid, 0) != tid) futex_pi(m, FUTEX_UNLOCK_PI, 0);
}

/* Acquire with the mutex's protocol, without the ceiling. */
static int base_lock(pthread_mutex_t *m, struct pthread *self, clockid_t clk, const struct timespec *at, int try)
{
	int type = m->__type, tid = self->tid, r;
	if (!tid_owned(m)) {
		r = try ? (a_cas(&m->__lock, 0, 1) ? EBUSY : 0) : lock_word(m, clk, at);
		if (!r && (type & MT_TYPE_MASK) != PTHREAD_MUTEX_NORMAL) m->__owner = tid;
		return r;
	}
	if (type & MT_NOTRECOVERABLE) return ENOTRECOVERABLE;
	if (type & MT_ROBUST) {
		robust_register(self);
		self->robust.pending = (void *)&m->__next;
	}
	r = type & MT_PI ? pi_lock_word(m, tid, clk, at, try) : robust_lock_word(m, tid, clk, at, try);
	if (r == 0 || r == EOWNERDEAD) {
		if (m->__type & MT_NOTRECOVERABLE) {
			/* made unrecoverable while we waited: pass it on */
			if (type & MT_PI) pi_unlock_word(m, tid);
			else if (a_swap(&m->__lock, 0) & FUTEX_WAITERS) __futex_wake(&m->__lock, INT_MAX, priv(m));
			r = ENOTRECOVERABLE;
		} else {
			if (type & MT_ROBUST) robust_add(self, m);
			if (r == EOWNERDEAD) a_fetch_or(&m->__type, MT_INCONSISTENT);
			m->__owner = tid;
		}
	}
	if (type & MT_ROBUST) self->robust.pending = 0;
	return r;
}

static void base_unlock(pthread_mutex_t *m, struct pthread *self)
{
	int type = m->__type;
	if (!tid_owned(m)) {
		if (a_swap(&m->__lock, 0) == 2) __futex_wake(&m->__lock, 1, priv(m));
		return;
	}
	int unrecoverable = 0;
	if (type & MT_ROBUST) {
		self->robust.pending = (void *)&m->__next;
		robust_del(self, m);
	}
	if (type & MT_INCONSISTENT) {
		/* unlocked without pthread_mutex_consistent: unusable for good */
		a_fetch_or(&m->__type, MT_NOTRECOVERABLE);
		a_fetch_and(&m->__type, ~MT_INCONSISTENT);
		unrecoverable = 1;
	}
	m->__owner = 0;
	if (type & MT_PI) {
		pi_unlock_word(m, self->tid);
	} else {
		int v = a_swap(&m->__lock, 0);
		if ((v & FUTEX_WAITERS) || unrecoverable)
			__futex_wake(&m->__lock, unrecoverable ? INT_MAX : 1, priv(m));
	}
	if (type & MT_ROBUST) self->robust.pending = 0;
}

/* ------------------------------------------------------ lock/unlock */

static int owner_of(const pthread_mutex_t *m)
{
	return tid_owned(m) ? (m->__lock & FUTEX_TID_MASK) : m->__owner;
}

static int lock_common(pthread_mutex_t *m, clockid_t clk, const struct timespec *at, int try)
{
	struct pthread *self = __self();
	int type = m->__type, kind = type & MT_TYPE_MASK;
	if (kind != PTHREAD_MUTEX_NORMAL || tid_owned(m)) {
		if (owner_of(m) == self->tid) {
			if (kind == PTHREAD_MUTEX_RECURSIVE) {
				if (m->__count == INT_MAX) return EAGAIN;
				m->__count++;
				return 0;
			}
			return try ? EBUSY : EDEADLK;
		}
	}
	if (type & MT_PP) {
		int r = pp_enter(self, m->__prio);
		if (r) return r;
	}
	int r = base_lock(m, self, clk, at, try);
	if (r == 0 || r == EOWNERDEAD) m->__count = 0;
	else if (type & MT_PP) pp_leave(self, m->__prio);
	return r;
}

hidden int __pthread_mutex_timedlock_internal(pthread_mutex_t *m, clockid_t clk, const struct timespec *at)
{
	if (!(m->__type & MT_KIND)) {
		if (!a_cas(&m->__lock, 0, 1)) return 0;
		return lock_word(m, clk, at);
	}
	return lock_common(m, clk, at, 0);
}

hidden int __pthread_mutex_lock_internal(pthread_mutex_t *m)
{
	return __pthread_mutex_timedlock_internal(m, CLOCK_REALTIME, 0);
}

int pthread_mutex_lock(pthread_mutex_t *m)
{
	if (!(m->__type & MT_KIND) && !a_cas(&m->__lock, 0, 1)) return 0;
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
	if (!(m->__type & MT_KIND)) return a_cas(&m->__lock, 0, 1) ? EBUSY : 0;
	return lock_common(m, CLOCK_REALTIME, 0, 1);
}

hidden int __pthread_mutex_unlock_internal(pthread_mutex_t *m)
{
	struct pthread *self = __self();
	int type = m->__type, kind = type & MT_TYPE_MASK;
	if (kind != PTHREAD_MUTEX_NORMAL || tid_owned(m)) {
		if (owner_of(m) != self->tid) return EPERM;
		if (kind == PTHREAD_MUTEX_RECURSIVE && m->__count) {
			m->__count--;
			return 0;
		}
	}
	if (kind != PTHREAD_MUTEX_NORMAL && !tid_owned(m)) m->__owner = 0;
	base_unlock(m, self);
	if (type & MT_PP) pp_leave(self, m->__prio);
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *m)
{
	/* normal mutexes: one exchange, a wake only if someone sleeps */
	if (!(m->__type & MT_KIND)) {
		if (a_swap(&m->__lock, 0) == 2) __futex_wake(&m->__lock, 1, priv(m));
		return 0;
	}
	return __pthread_mutex_unlock_internal(m);
}

int pthread_mutex_consistent(pthread_mutex_t *m)
{
	if (!(m->__type & MT_ROBUST) || !(m->__type & MT_INCONSISTENT)) return EINVAL;
	if ((m->__lock & FUTEX_TID_MASK) != __self()->tid) return EPERM;
	a_fetch_and(&m->__type, ~MT_INCONSISTENT);
	return 0;
}

/* ------------------------------------------------------- ceilings */

int pthread_mutex_getprioceiling(const pthread_mutex_t *restrict m, int *restrict c)
{
	if (!(m->__type & MT_PP)) return EINVAL;
	*c = m->__prio;
	return 0;
}

/* Changes the ceiling with the mutex held (taken without applying the
 * old ceiling, which the caller's priority may exceed). */
int pthread_mutex_setprioceiling(pthread_mutex_t *restrict m, int c, int *restrict old)
{
	if (!(m->__type & MT_PP) || c < 1 || c > PRIO_MAX_RT) return EINVAL;
	struct pthread *self = __self();
	int tracked = (m->__type & MT_TYPE_MASK) != PTHREAD_MUTEX_NORMAL || tid_owned(m);
	int mine = tracked && owner_of(m) == self->tid;
	if (!mine) {
		int r = base_lock(m, self, CLOCK_REALTIME, 0, 0);
		if (r && r != EOWNERDEAD) return r;
	}
	if (old) *old = m->__prio;
	if (mine) {
		/* the caller holds it: move its count to the new ceiling */
		int prev = m->__prio;
		pp_leave(self, prev);
		int r = pp_enter(self, c);
		if (r) {
			pp_enter(self, prev);
			return r;
		}
		m->__prio = c;
	} else {
		m->__prio = c;
		base_unlock(m, self);
	}
	return 0;
}
