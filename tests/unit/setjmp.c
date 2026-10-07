/* lib-spfxd test — setjmp/longjmp, sigsetjmp, ucontext. */
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <ucontext.h>
#include "t.h"

static jmp_buf jb;
static int depth;
static void deep(int n) { if (n == 0) longjmp(jb, 0); deep(n - 1); depth++; }

static ucontext_t main_ctx, co_ctx;
static int co_steps;
static void coroutine(int a, int b)
{
	co_steps = a + b;
	swapcontext(&co_ctx, &main_ctx);
	co_steps *= 10;
}

int main(void)
{
	volatile int count = 0;
	int r = setjmp(jb);
	if (r == 0) {
		count++;
		deep(100);
	}
	CHECK(r == 1 && count == 1 && depth == 0, "longjmp(0) returns 1 from deep recursion");
	volatile double f = 1.5;
	if (!setjmp(jb)) { f = 2.5; longjmp(jb, 7); }
	CHECK(f == 2.5, "volatile locals survive");
	/* sigsetjmp saves and restores the mask when asked */
	sigjmp_buf sj;
	sigset_t s, cur;
	sigemptyset(&s);
	sigaddset(&s, SIGUSR1);
	if (!sigsetjmp(sj, 1)) {
		sigprocmask(SIG_BLOCK, &s, 0);
		siglongjmp(sj, 1);
	}
	sigprocmask(SIG_BLOCK, 0, &cur);
	CHECK(!sigismember(&cur, SIGUSR1), "sigsetjmp(1) restores mask");
	if (!sigsetjmp(sj, 0)) {
		sigprocmask(SIG_BLOCK, &s, 0);
		siglongjmp(sj, 1);
	}
	sigprocmask(SIG_BLOCK, 0, &cur);
	CHECK(sigismember(&cur, SIGUSR1), "sigsetjmp(0) leaves mask");
	sigprocmask(SIG_UNBLOCK, &s, 0);
	_setjmp(jb) ? (void)0 : _longjmp(jb, 3);
	/* ucontext coroutine */
	static char stack[64 * 1024];
	CHECK(!getcontext(&co_ctx), "getcontext");
	co_ctx.uc_stack.ss_sp = stack;
	co_ctx.uc_stack.ss_size = sizeof stack;
	co_ctx.uc_link = &main_ctx;
	makecontext(&co_ctx, (void (*)(void))coroutine, 2, 3, 4);
	CHECK(!swapcontext(&main_ctx, &co_ctx) && co_steps == 7, "makecontext args, swap out");
	CHECK(!swapcontext(&main_ctx, &co_ctx) && co_steps == 70, "resume, uc_link return");
	/* getcontext/setcontext loop */
	volatile int loops = 0;
	ucontext_t lc;
	getcontext(&lc);
	if (++loops < 3) setcontext(&lc);
	CHECK(loops == 3, "setcontext loop");
	return DONE();
}
