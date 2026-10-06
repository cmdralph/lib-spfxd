/*
 * lib-spfxd — sigaction.
 *
 * The kernel's struct sigaction has a different layout from the user one
 * (handler, flags, restorer, 64-bit mask).  Every handler gets
 * SA_RESTORER with __restore_rt, the trampoline that performs
 * rt_sigreturn when the handler returns.
 */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "pthread_impl.h"

struct k_sigaction {
	void (*handler)(int);
	unsigned long flags;
	void (*restorer)(void);
	unsigned long mask;
};

hidden void __restore_rt(void);

hidden int __libc_sigaction(int sig, const struct sigaction *restrict sa,
	struct sigaction *restrict old)
{
	struct k_sigaction ksa, kold;
	if (sa) {
		ksa.handler = sa->sa_handler;
		ksa.flags = (unsigned long)(unsigned)sa->sa_flags | SA_RESTORER;
		ksa.restorer = __restore_rt;
		ksa.mask = sa->sa_mask.__bits[0];
	}
	long r = __syscall(SYS_rt_sigaction, sig, sa ? &ksa : 0, old ? &kold : 0, 8);
	if (r < 0) return (int)r;
	if (old) {
		memset(old, 0, sizeof *old);
		old->sa_handler = kold.handler;
		old->sa_flags = (int)kold.flags;
		old->sa_restorer = kold.restorer;
		old->sa_mask.__bits[0] = kold.mask;
	}
	return 0;
}

int sigaction(int sig, const struct sigaction *restrict sa, struct sigaction *restrict old)
{
	if (sig < 1 || sig >= _NSIG || sig == SIGCANCEL || sig == SIGSYNCCALL) {
		errno = EINVAL;
		return -1;
	}
	if (sa && (sig == SIGKILL || sig == SIGSTOP)) {
		errno = EINVAL;
		return -1;
	}
	struct sigaction tmp;
	if (sa) {
		tmp = *sa;
		__sig_strip_internal(&tmp.sa_mask);
		sa = &tmp;
	}
	return (int)__syscall_ret((unsigned long)__libc_sigaction(sig, sa, old));
}
