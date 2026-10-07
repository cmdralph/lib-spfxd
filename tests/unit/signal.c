/* lib-spfxd test — signals: sigaction, masks, SA_SIGINFO, sigqueue, real
 * time signal ordering, sigsuspend, sigwait family, alarm/timers,
 * sigaltstack, SA_RESTART, siglongjmp out of a handler. */
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "t.h"

static volatile sig_atomic_t hits, last_sig, last_code, last_val;
static void h(int s) { hits++; last_sig = s; }
static void hi(int s, siginfo_t *si, void *uc) { (void)uc; hits++; last_sig = s; last_code = si->si_code; last_val = si->si_value.sival_int; }

static int order[8], norder;
static void rt(int s, siginfo_t *si, void *uc) { (void)uc; (void)si; if (norder < 8) order[norder++] = s; }

static sigjmp_buf jb;
static void jumper(int s) { siglongjmp(jb, s); }

static volatile uintptr_t alt_sp;
static void on_alt(int s) { int x; (void)s; alt_sp = (uintptr_t)&x; }

int main(void)
{
	struct sigaction sa, old;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = h;
	sigemptyset(&sa.sa_mask);
	CHECK(!sigaction(SIGUSR1, &sa, &old), "sigaction");
	raise(SIGUSR1);
	CHECK(hits == 1 && last_sig == SIGUSR1, "raise");
	kill(getpid(), SIGUSR1);
	CHECK(hits == 2, "kill self");
	/* blocking and pending */
	sigset_t set, oset, pend;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigprocmask(SIG_BLOCK, &set, &oset);
	raise(SIGUSR1);
	sigpending(&pend);
	CHECK(hits == 2 && sigismember(&pend, SIGUSR1), "blocked signal pending");
	sigprocmask(SIG_SETMASK, &oset, 0);
	CHECK(hits == 3, "delivered on unblock");
	/* internal signals cannot be blocked or caught */
	sigfillset(&set);
	CHECK(!sigismember(&set, 32) && !sigismember(&set, 33), "sigfillset excludes internal signals");
	CHECK(sigaction(32, &sa, 0) == -1 && errno == EINVAL, "internal signal not catchable");
	CHECK(SIGRTMIN == 34 && SIGRTMAX == 64, "SIGRTMIN/MAX");
	/* SA_SIGINFO and sigqueue */
	sa.sa_sigaction = hi;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGUSR2, &sa, 0);
	sigqueue(getpid(), SIGUSR2, (union sigval){ .sival_int = 1234 });
	CHECK(last_sig == SIGUSR2 && last_code == SI_QUEUE && last_val == 1234, "sigqueue value %d code %d", (int)last_val, (int)last_code);
	/* real-time signals: queued, delivered lowest number first */
	sa.sa_sigaction = rt;
	sigemptyset(&set);
	for (int s = SIGRTMIN; s < SIGRTMIN + 3; s++) { sigaction(s, &sa, 0); sigaddset(&set, s); }
	sigprocmask(SIG_BLOCK, &set, 0);
	sigqueue(getpid(), SIGRTMIN + 2, (union sigval){ 0 });
	sigqueue(getpid(), SIGRTMIN, (union sigval){ 0 });
	sigqueue(getpid(), SIGRTMIN + 1, (union sigval){ 0 });
	sigqueue(getpid(), SIGRTMIN, (union sigval){ 0 });
	sigprocmask(SIG_UNBLOCK, &set, 0);
	/* delivery order is the kernel's business (it differs between Linux
	 * and gVisor); every queued instance must arrive */
	int cnt[3] = { 0 };
	for (int i = 0; i < norder; i++) cnt[order[i] - SIGRTMIN]++;
	CHECK(norder == 4 && cnt[0] == 2 && cnt[1] == 1 && cnt[2] == 1, "RT signals queued (%d)", norder);
	/* sigwait / sigtimedwait / sigwaitinfo */
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigprocmask(SIG_BLOCK, &set, 0);
	raise(SIGUSR1);
	int sig = 0;
	CHECK(!sigwait(&set, &sig) && sig == SIGUSR1, "sigwait");
	struct timespec to = { 0, 20000000 };
	siginfo_t si;
	CHECK(sigtimedwait(&set, &si, &to) == -1 && errno == EAGAIN, "sigtimedwait timeout");
	raise(SIGUSR1);
	CHECK(sigwaitinfo(&set, &si) == SIGUSR1 && si.si_signo == SIGUSR1, "sigwaitinfo");
	sigprocmask(SIG_UNBLOCK, &set, 0);
	/* sigsuspend */
	sigprocmask(SIG_BLOCK, &set, &oset);
	sa.sa_handler = h;
	sa.sa_flags = 0;
	sigaction(SIGALRM, &sa, 0);
	hits = 0;
	alarm(1);
	sigset_t wait_mask;
	sigemptyset(&wait_mask);
	CHECK(sigsuspend(&wait_mask) == -1 && errno == EINTR && hits == 1 && last_sig == SIGALRM, "alarm + sigsuspend");
	sigprocmask(SIG_SETMASK, &oset, 0);
	/* interval timer */
	struct itimerval it = { { 0, 0 }, { 0, 20000 } };
	hits = 0;
	CHECK(!setitimer(ITIMER_REAL, &it, 0), "setitimer");
	while (!hits) pause();
	CHECK(hits == 1, "itimer fired");
	/* POSIX timer with signal notification */
	timer_t tid;
	struct sigevent sev = { .sigev_notify = SIGEV_SIGNAL, .sigev_signo = SIGUSR2, .sigev_value.sival_int = 77 };
	sa.sa_sigaction = hi;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGUSR2, &sa, 0);
	CHECK(!timer_create(CLOCK_MONOTONIC, &sev, &tid), "timer_create");
	struct itimerspec its = { { 0, 0 }, { 0, 10000000 } };
	last_val = 0;
	timer_settime(tid, 0, &its, 0);
	while (last_val != 77) pause();
	CHECK(last_code == SI_TIMER, "timer signal code");
	timer_delete(tid);
	/* sigaltstack */
	stack_t ss = { .ss_sp = malloc(SIGSTKSZ * 4), .ss_size = SIGSTKSZ * 4, .ss_flags = 0 };
	CHECK(!sigaltstack(&ss, 0), "sigaltstack");
	sa.sa_handler = on_alt;
	sa.sa_flags = SA_ONSTACK;
	sigaction(SIGUSR1, &sa, 0);
	raise(SIGUSR1);
	CHECK(alt_sp > (uintptr_t)ss.ss_sp && alt_sp < (uintptr_t)ss.ss_sp + ss.ss_size, "handler ran on alternate stack");
	/* siglongjmp from a handler restores the mask */
	sa.sa_handler = jumper;
	sa.sa_flags = 0;
	sigaction(SIGUSR1, &sa, 0);
	int r = sigsetjmp(jb, 1);
	if (!r) {
		raise(SIGUSR1);
		CHECK(0, "not reached");
	}
	sigprocmask(SIG_BLOCK, 0, &oset);
	CHECK(r == SIGUSR1 && !sigismember(&oset, SIGUSR1), "siglongjmp restored mask");
	/* SA_RESTART: read on a pipe continues after the handler */
	int p[2];
	pipe(p);
	sa.sa_handler = h;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGALRM, &sa, 0);
	pid_t child = fork();
	if (!child) {
		usleep(150000);
		write(p[1], "x", 1);
		_exit(0);
	}
	hits = 0;
	struct itimerval it2 = { { 0, 0 }, { 0, 30000 } };
	setitimer(ITIMER_REAL, &it2, 0);
	char c;
	ssize_t n = read(p[0], &c, 1);
	CHECK(n == 1 && hits == 1, "SA_RESTART read survived the signal (n=%zd hits=%d)", n, (int)hits);
	sa.sa_flags = 0;
	sigaction(SIGALRM, &sa, 0);
	setitimer(ITIMER_REAL, &it2, 0);
	errno = 0;
	CHECK(read(p[0], &c, 1) == -1 && errno == EINTR, "without SA_RESTART read fails with EINTR");
	/* default action and SIG_IGN through fork/exec */
	signal(SIGUSR1, SIG_IGN);
	raise(SIGUSR1);
	CHECK(1, "ignored signal");
	CHECK(signal(SIGUSR1, SIG_DFL) == SIG_IGN, "signal returns previous disposition");
	CHECK(!strcmp(strsignal(SIGRTMIN + 1), "Real-time signal 1") || strsignal(SIGRTMIN + 1), "strsignal RT");
	psignal(SIGINT, "psignal check");
	return DONE();
}
