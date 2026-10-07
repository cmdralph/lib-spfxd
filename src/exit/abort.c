/*
 * lib-spfxd — abort().
 *
 * Raise SIGABRT normally first so that an installed handler runs.  If the
 * handler returns (or the signal is ignored or blocked), force the default
 * action: block everything, reset the disposition, then deliver SIGABRT to
 * this thread with only SIGABRT unblocked.  Should even that fail the
 * process is crashed rather than allowed to continue.
 */
#include <stdlib.h>
#include <signal.h>
#include "libc.h"
#include "pthread_impl.h"

struct k_sigaction_min {
	void (*handler)(int);
	unsigned long flags;
	void (*restorer)(void);
	unsigned long mask;
};

_Noreturn void abort(void)
{
	raise(SIGABRT);

	unsigned long all = ~0UL, abrt = 1UL << (SIGABRT - 1);
	__syscall(SYS_rt_sigprocmask, SIG_BLOCK, &all, 0, 8);
	struct k_sigaction_min sa = { SIG_DFL, 0, 0, 0 };
	__syscall(SYS_rt_sigaction, SIGABRT, &sa, 0, 8);
	__syscall(SYS_tkill, __self()->tid, SIGABRT);
	__syscall(SYS_rt_sigprocmask, SIG_UNBLOCK, &abrt, 0, 8);
	a_crash();
}
