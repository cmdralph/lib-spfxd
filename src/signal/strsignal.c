/* lib-spfxd — signal descriptions: strsignal, psignal, psiginfo,
 * sys_siglist, sigdescr_np, sigabbrev_np. */
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include "pthread_impl.h"

const char *const sys_siglist[_NSIG] = {
	[0] = "Unknown signal",
	[SIGHUP] = "Hangup",
	[SIGINT] = "Interrupt",
	[SIGQUIT] = "Quit",
	[SIGILL] = "Illegal instruction",
	[SIGTRAP] = "Trace/breakpoint trap",
	[SIGABRT] = "Aborted",
	[SIGBUS] = "Bus error",
	[SIGFPE] = "Floating point exception",
	[SIGKILL] = "Killed",
	[SIGUSR1] = "User defined signal 1",
	[SIGSEGV] = "Segmentation fault",
	[SIGUSR2] = "User defined signal 2",
	[SIGPIPE] = "Broken pipe",
	[SIGALRM] = "Alarm clock",
	[SIGTERM] = "Terminated",
	[SIGSTKFLT] = "Stack fault",
	[SIGCHLD] = "Child exited",
	[SIGCONT] = "Continued",
	[SIGSTOP] = "Stopped (signal)",
	[SIGTSTP] = "Stopped",
	[SIGTTIN] = "Stopped (tty input)",
	[SIGTTOU] = "Stopped (tty output)",
	[SIGURG] = "Urgent I/O condition",
	[SIGXCPU] = "CPU time limit exceeded",
	[SIGXFSZ] = "File size limit exceeded",
	[SIGVTALRM] = "Virtual timer expired",
	[SIGPROF] = "Profiling timer expired",
	[SIGWINCH] = "Window changed",
	[SIGIO] = "I/O possible",
	[SIGPWR] = "Power failure",
	[SIGSYS] = "Bad system call",
};

static const char abbrev[32][8] = {
	"", "HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT", "BUS", "FPE", "KILL",
	"USR1", "SEGV", "USR2", "PIPE", "ALRM", "TERM", "STKFLT", "CHLD", "CONT",
	"STOP", "TSTP", "TTIN", "TTOU", "URG", "XCPU", "XFSZ", "VTALRM", "PROF",
	"WINCH", "IO", "PWR", "SYS",
};

const char *sigdescr_np(int sig)
{
	if (sig > 0 && sig < 32) return sys_siglist[sig];
	return 0;
}

const char *sigabbrev_np(int sig)
{
	if (sig > 0 && sig < 32) return abbrev[sig];
	return 0;
}

char *strsignal(int sig)
{
	if (sig > 0 && sig < 32) return (char *)sys_siglist[sig];
	char *buf = __self()->strerror_buf;
	if (sig >= SIGRT_FIRST_USER && sig < _NSIG)
		snprintf(buf, sizeof __self()->strerror_buf, "Real-time signal %d", sig - SIGRT_FIRST_USER);
	else
		snprintf(buf, sizeof __self()->strerror_buf, "Unknown signal %d", sig);
	return buf;
}

void psignal(int sig, const char *msg)
{
	if (msg && *msg) fprintf(stderr, "%s: %s\n", msg, strsignal(sig));
	else fprintf(stderr, "%s\n", strsignal(sig));
}

void psiginfo(const siginfo_t *si, const char *msg)
{
	psignal(si->si_signo, msg);
}
