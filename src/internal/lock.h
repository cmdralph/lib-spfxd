/*
 * lib-spfxd — futexes and the internal lock.
 *
 * Internal lock protocol (word values):
 *   0  unlocked
 *   1  locked, no waiters
 *   2  locked, possibly with waiters (unlock must issue FUTEX_WAKE)
 * Acquire: CAS 0->1; on contention spin briefly, then swap in 2 and sleep
 * while the swap returns non-zero.  Release: swap in 0; wake one waiter only
 * if the old value was 2.  The uncontended paths are a single locked
 * instruction each and never enter the kernel.
 *
 * Internal locks are skipped entirely while the process is single-threaded.
 */
#ifndef _SPFXD_LOCK_H
#define _SPFXD_LOCK_H

#include <time.h>
#include "libc.h"
#include "atomic.h"
#include "syscall.h"

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_REQUEUE 3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAIT_BITSET 9
#define FUTEX_PRIVATE 128
#define FUTEX_CLOCK_REALTIME 256
#define FUTEX_BITSET_ANY 0xffffffff

static __inline void __futex_wake(volatile void *addr, int cnt, int priv)
{
	if (cnt < 0) cnt = 0x7fffffff;
	long r = __syscall(SYS_futex, addr, FUTEX_WAKE | (priv ? FUTEX_PRIVATE : 0), cnt);
	if (r == -38 /* ENOSYS: kernel without private futexes */)
		__syscall(SYS_futex, addr, FUTEX_WAKE, cnt);
}

static __inline void __futex_wait(volatile void *addr, int val, int priv)
{
	long r = __syscall(SYS_futex, addr, FUTEX_WAIT | (priv ? FUTEX_PRIVATE : 0), val, 0);
	if (r == -38)
		__syscall(SYS_futex, addr, FUTEX_WAIT, val, 0);
}

/* Wait while *addr == val until woken, interrupted or the absolute time
 * `at` (on clock `clk`) passes.  Returns 0, ETIMEDOUT, EINTR or EINVAL.
 * When `cp` is nonzero the wait is a cancellation point. */
hidden int __timedwait(volatile int *addr, int val, clockid_t clk,
	const struct timespec *at, int priv, int cp);

hidden void __lock_slow(volatile int *l);
hidden void __unlock_wake(volatile int *l);

static __inline void __lock(volatile int *l)
{
	if (!__libc.threaded) return;
	if (__builtin_expect(a_cas(l, 0, 1) != 0, 0))
		__lock_slow(l);
}

static __inline void __unlock(volatile int *l)
{
	if (!__libc.threaded) {
		/* A lock taken via __lock_always while single-threaded may still
		 * be marked; clear it without a wake. */
		if (*l) *l = 0;
		return;
	}
	if (a_swap(l, 0) == 2)
		__unlock_wake(l);
}

/* Variants that always lock, for state shared with code that may run
 * before or across the first pthread_create. */
static __inline void __lock_always(volatile int *l)
{
	if (__builtin_expect(a_cas(l, 0, 1) != 0, 0))
		__lock_slow(l);
}
static __inline void __unlock_always(volatile int *l)
{
	if (a_swap(l, 0) == 2)
		__unlock_wake(l);
}

#define LOCK(x) __lock(x)
#define UNLOCK(x) __unlock(x)

#endif
