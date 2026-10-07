/* lib-spfxd — signal(): BSD semantics (handler stays installed, interrupted
 * system calls restart), plus the XSI legacy interfaces built on sigaction. */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "pthread_impl.h"

void (*signal(int sig, void (*func)(int)))(int)
{
	struct sigaction sa, old;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = func;
	sa.sa_flags = SA_RESTART;
	if (sigaction(sig, &sa, &old) < 0) return SIG_ERR;
	return old.sa_handler;
}
weak_alias(signal, bsd_signal);

int siginterrupt(int sig, int flag)
{
	struct sigaction sa;
	if (sigaction(sig, 0, &sa) < 0) return -1;
	if (flag) sa.sa_flags &= ~SA_RESTART;
	else sa.sa_flags |= SA_RESTART;
	return sigaction(sig, &sa, 0);
}

static int mask_one(int how, int sig)
{
	sigset_t s;
	sigemptyset(&s);
	if (sigaddset(&s, sig) < 0) return -1;
	return sigprocmask(how, &s, 0);
}

int sighold(int sig) { return mask_one(SIG_BLOCK, sig); }
int sigrelse(int sig) { return mask_one(SIG_UNBLOCK, sig); }

int sigignore(int sig)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = SIG_IGN;
	return sigaction(sig, &sa, 0);
}

int sigpause(int sig)
{
	sigset_t s;
	sigprocmask(SIG_BLOCK, 0, &s);
	if (sigdelset(&s, sig) < 0) return -1;
	return sigsuspend(&s);
}

void (*sigset(int sig, void (*handler)(int)))(int)
{
	struct sigaction sa, old;
	sigset_t mask, oldmask;
	sigemptyset(&mask);
	if (sigaddset(&mask, sig) < 0) return SIG_ERR;
	if (handler == SIG_HOLD) {
		if (sigaction(sig, 0, &old) < 0) return SIG_ERR;
		if (sigprocmask(SIG_BLOCK, &mask, &oldmask) < 0) return SIG_ERR;
	} else {
		memset(&sa, 0, sizeof sa);
		sa.sa_handler = handler;
		if (sigaction(sig, &sa, &old) < 0) return SIG_ERR;
		if (sigprocmask(SIG_UNBLOCK, &mask, &oldmask) < 0) return SIG_ERR;
	}
	return sigismember(&oldmask, sig) ? SIG_HOLD : old.sa_handler;
}
