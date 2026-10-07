/* lib-spfxd — raise / kill / killpg / sigqueue / pause. */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "pthread_impl.h"

/* Signals are blocked around the tkill so the signal is delivered (and any
 * handler has run) by the time the mask is restored, i.e. before raise
 * returns, as ISO C requires. */
int raise(int sig)
{
	sigset_t set;
	__block_app_sigs(&set);
	int r = (int)__sysret(SYS_tkill, __self()->tid, sig);
	__restore_sigs(&set);
	return r;
}

int kill(pid_t pid, int sig)
{
	return (int)__sysret(SYS_kill, pid, sig);
}

int killpg(pid_t pgrp, int sig)
{
	if (pgrp < 0) {
		errno = EINVAL;
		return -1;
	}
	return kill(-pgrp, sig);
}

int sigqueue(pid_t pid, int sig, union sigval value)
{
	siginfo_t si;
	sigset_t set;
	memset(&si, 0, sizeof si);
	si.si_signo = sig;
	si.si_code = SI_QUEUE;
	si.si_value = value;
	si.si_uid = (uid_t)__syscall(SYS_getuid);
	__block_app_sigs(&set);
	si.si_pid = (pid_t)__syscall(SYS_getpid);
	int r = (int)__sysret(SYS_rt_sigqueueinfo, pid, sig, &si);
	__restore_sigs(&set);
	return r;
}

int pause(void)
{
	return (int)__sysret_cp(SYS_pause);
}
