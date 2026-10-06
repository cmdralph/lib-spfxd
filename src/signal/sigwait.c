/* lib-spfxd — sigwait / sigwaitinfo / sigtimedwait. */
#include <errno.h>
#include <signal.h>
#include "pthread_impl.h"

int sigtimedwait(const sigset_t *restrict mask, siginfo_t *restrict si,
	const struct timespec *restrict timeout)
{
	sigset_t m;
	m.__bits[0] = mask->__bits[0];
	__sig_strip_internal(&m);
	long r;
	do r = __syscall_cp(SYS_rt_sigtimedwait, &m, si, timeout, 8);
	while (r == -EINTR);
	return (int)__syscall_ret((unsigned long)r);
}

int sigwaitinfo(const sigset_t *restrict mask, siginfo_t *restrict si)
{
	return sigtimedwait(mask, si, 0);
}

int sigwait(const sigset_t *restrict mask, int *restrict sig)
{
	siginfo_t si;
	if (sigtimedwait(mask, &si, 0) < 0) return errno;
	*sig = si.si_signo;
	return 0;
}
