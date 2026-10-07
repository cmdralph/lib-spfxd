/* lib-spfxd — sigprocmask / pthread_sigmask / sigpending / sigsuspend. */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "pthread_impl.h"

int pthread_sigmask(int how, const sigset_t *restrict set, sigset_t *restrict old)
{
	sigset_t tmp;
	if (set) {
		if ((unsigned)how > SIG_SETMASK) return EINVAL;
		tmp.__bits[0] = set->__bits[0];
		__sig_strip_internal(&tmp);
		set = &tmp;
	}
	if (old) memset(old, 0, sizeof *old);
	long r = __syscall(SYS_rt_sigprocmask, how, set, old, 8);
	if (!r && old) __sig_strip_internal(old);
	return (int)-r;
}

int sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict old)
{
	int r = pthread_sigmask(how, set, old);
	if (!r) return 0;
	errno = r;
	return -1;
}

int sigpending(sigset_t *set)
{
	memset(set, 0, sizeof *set);
	return (int)__sysret(SYS_rt_sigpending, set, 8);
}

int sigsuspend(const sigset_t *mask)
{
	sigset_t tmp;
	tmp.__bits[0] = mask->__bits[0];
	__sig_strip_internal(&tmp);
	return (int)__sysret_cp(SYS_rt_sigsuspend, &tmp, 8);
}
