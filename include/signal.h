/* lib-spfxd — <signal.h> (Linux signal ABI) */
#ifndef _SIGNAL_H
#define _SIGNAL_H
#include <features.h>

#if defined(__SPFXD_POSIX)
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_uid_t
#define __SPFXD_NEED_sigset_t
#define __SPFXD_NEED_pthread_t
#define __SPFXD_NEED_pthread_types
#define __SPFXD_NEED_struct_timespec
#define __SPFXD_NEED_clock_t
#endif
#include <bits/typedefs.h>

typedef int sig_atomic_t;

#define SIG_ERR  ((void (*)(int))-1)
#define SIG_DFL  ((void (*)(int)) 0)
#define SIG_IGN  ((void (*)(int)) 1)

#define SIGHUP     1
#define SIGINT     2
#define SIGQUIT    3
#define SIGILL     4
#define SIGTRAP    5
#define SIGABRT    6
#define SIGIOT     SIGABRT
#define SIGBUS     7
#define SIGFPE     8
#define SIGKILL    9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPOLL   SIGIO
#define SIGPWR    30
#define SIGSYS    31
#define SIGUNUSED SIGSYS
#define _NSIG     65
#define NSIG      _NSIG

__SPFXD_BEGIN_DECLS
/* Signals 32 and 33 are reserved for the library's internal use. */
int __libc_current_sigrtmin(void);
int __libc_current_sigrtmax(void);
#define SIGRTMIN  (__libc_current_sigrtmin())
#define SIGRTMAX  (__libc_current_sigrtmax())

typedef void (*__sighandler_t)(int);
void (*signal(int, void (*)(int)))(int);
int raise(int);

#if defined(__SPFXD_POSIX)
#define SIG_HOLD ((void (*)(int)) 2)

#define SA_NOCLDSTOP  0x00000001
#define SA_NOCLDWAIT  0x00000002
#define SA_SIGINFO    0x00000004
#define SA_RESTORER   0x04000000
#define SA_ONSTACK    0x08000000
#define SA_RESTART    0x10000000
#define SA_NODEFER    0x40000000
#define SA_RESETHAND  0x80000000
#define SA_NOMASK     SA_NODEFER
#define SA_ONESHOT    SA_RESETHAND

#define SIG_BLOCK     0
#define SIG_UNBLOCK   1
#define SIG_SETMASK   2

#define SS_ONSTACK    1
#define SS_DISABLE    2
#define SS_AUTODISARM (1U << 31)

#include <bits/siginfo.h>

#define SI_ASYNCNL (-60)
#define SI_TKILL   (-6)
#define SI_SIGIO   (-5)
#define SI_ASYNCIO (-4)
#define SI_MESGQ   (-3)
#define SI_TIMER   (-2)
#define SI_QUEUE   (-1)
#define SI_USER    0
#define SI_KERNEL  128

#define ILL_ILLOPC 1
#define ILL_ILLOPN 2
#define ILL_ILLADR 3
#define ILL_ILLTRP 4
#define ILL_PRVOPC 5
#define ILL_PRVREG 6
#define ILL_COPROC 7
#define ILL_BADSTK 8
#define FPE_INTDIV 1
#define FPE_INTOVF 2
#define FPE_FLTDIV 3
#define FPE_FLTOVF 4
#define FPE_FLTUND 5
#define FPE_FLTRES 6
#define FPE_FLTINV 7
#define FPE_FLTSUB 8
#define SEGV_MAPERR 1
#define SEGV_ACCERR 2
#define SEGV_BNDERR 3
#define SEGV_PKUERR 4
#define BUS_ADRALN 1
#define BUS_ADRERR 2
#define BUS_OBJERR 3
#define BUS_MCEERR_AR 4
#define BUS_MCEERR_AO 5
#define TRAP_BRKPT 1
#define TRAP_TRACE 2
#define TRAP_BRANCH 3
#define TRAP_HWBKPT 4
#define CLD_EXITED 1
#define CLD_KILLED 2
#define CLD_DUMPED 3
#define CLD_TRAPPED 4
#define CLD_STOPPED 5
#define CLD_CONTINUED 6
#define POLL_IN  1
#define POLL_OUT 2
#define POLL_MSG 3
#define POLL_ERR 4
#define POLL_PRI 5
#define POLL_HUP 6

struct sigaction {
	union {
		void (*sa_handler)(int);
		void (*sa_sigaction)(int, siginfo_t *, void *);
	} __sa_handler;
	sigset_t sa_mask;
	int sa_flags;
	void (*sa_restorer)(void);
};
#define sa_handler   __sa_handler.sa_handler
#define sa_sigaction __sa_handler.sa_sigaction


int kill(pid_t, int);
int sigaction(int, const struct sigaction *__restrict, struct sigaction *__restrict);
int sigprocmask(int, const sigset_t *__restrict, sigset_t *__restrict);
int sigpending(sigset_t *);
int sigsuspend(const sigset_t *);
int sigemptyset(sigset_t *);
int sigfillset(sigset_t *);
int sigaddset(sigset_t *, int);
int sigdelset(sigset_t *, int);
int sigismember(const sigset_t *, int);
int sigwait(const sigset_t *__restrict, int *__restrict);
int sigwaitinfo(const sigset_t *__restrict, siginfo_t *__restrict);
int sigtimedwait(const sigset_t *__restrict, siginfo_t *__restrict, const struct timespec *__restrict);
int sigqueue(pid_t, int, union sigval);
int pthread_sigmask(int, const sigset_t *__restrict, sigset_t *__restrict);
int pthread_kill(pthread_t, int);
void psiginfo(const siginfo_t *, const char *);
void psignal(int, const char *);
int sigaltstack(const stack_t *__restrict, stack_t *__restrict);
#endif

#if defined(__SPFXD_XSI)
int killpg(pid_t, int);
int siginterrupt(int, int);
int sighold(int);
int sigignore(int);
int sigpause(int);
int sigrelse(int);
void (*sigset(int, void (*)(int)))(int);
#endif

#if defined(__SPFXD_BSD) || defined(__SPFXD_GNU)
typedef void (*sig_t)(int);
typedef void (*sighandler_t)(int);
extern const char *const sys_siglist[_NSIG];
int sigisemptyset(const sigset_t *);
int sigorset(sigset_t *, const sigset_t *, const sigset_t *);
int sigandset(sigset_t *, const sigset_t *, const sigset_t *);
#endif
__SPFXD_END_DECLS
#endif
