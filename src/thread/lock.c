/* lib-spfxd — internal lock slow paths and futex timed waits. */
#include <errno.h>
#include <time.h>
#include "pthread_impl.h"

hidden void __lock_slow(volatile int *l)
{
	/* Brief optimistic spin: most internal critical sections are a few
	 * dozen instructions, so the holder usually releases quickly. */
	for (int i = 0; i < 64; i++) {
		if (!*l && !a_cas(l, 0, 1)) return;
		a_spin();
	}
	while (a_swap(l, 2)) __futex_wait(l, 2, 1);
}

hidden void __unlock_wake(volatile int *l)
{
	__futex_wake(l, 1, 1);
}

hidden int __timedwait(volatile int *addr, int val, clockid_t clk,
	const struct timespec *at, int priv, int cp)
{
	struct timespec rel, *ts = 0;
	int op = FUTEX_WAIT_BITSET;
	long r;

	if (at) {
		if ((unsigned long)at->tv_nsec >= 1000000000UL) return EINVAL;
		if (clk == CLOCK_REALTIME) {
			op |= FUTEX_CLOCK_REALTIME;
			ts = (struct timespec *)at;
		} else if (clk == CLOCK_MONOTONIC) {
			ts = (struct timespec *)at;
		} else {
			/* Other clocks: convert to a relative wait. */
			struct timespec now;
			if (clock_gettime(clk, &now)) return EINVAL;
			rel.tv_sec = at->tv_sec - now.tv_sec;
			rel.tv_nsec = at->tv_nsec - now.tv_nsec;
			if (rel.tv_nsec < 0) {
				rel.tv_nsec += 1000000000;
				rel.tv_sec--;
			}
			if (rel.tv_sec < 0) return ETIMEDOUT;
			op = FUTEX_WAIT;
			ts = &rel;
		}
	}
	if (priv) op |= FUTEX_PRIVATE;
	if (cp)
		r = -__syscall_cp(SYS_futex, addr, op, val, ts, 0, FUTEX_BITSET_ANY);
	else
		r = -__syscall(SYS_futex, addr, op, val, ts, 0, FUTEX_BITSET_ANY);
	if (r == ENOSYS && priv) {
		op &= ~FUTEX_PRIVATE;
		r = -__syscall(SYS_futex, addr, op, val, ts, 0, FUTEX_BITSET_ANY);
	}
	if (r != EINTR && r != ETIMEDOUT && r != ECANCELED) r = 0;
	return (int)r;
}
