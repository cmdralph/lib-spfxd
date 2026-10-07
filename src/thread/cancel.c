/*
 * lib-spfxd — thread cancellation.
 *
 * Deferred cancellation acts only inside cancellation points, which are the
 * system calls issued through __syscall_cp (see arch syscall_cp.S for the
 * pc-window technique that makes the check-then-block sequence race free).
 * pthread_cancel sets the target's flag and sends SIGCANCEL; its handler
 * redirects a thread that is inside the window to __cp_cancel, acts
 * immediately in asynchronous mode, and otherwise leaves the request
 * pending for the next cancellation point.
 *
 * Nothing here costs anything until the first pthread_cancel: before that
 * __syscall_cp is an ordinary system call.
 */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "pthread_impl.h"

extern hidden const char __cp_begin[1], __cp_end[1], __cp_cancel[1];

/* Reached by jumping from __cp_cancel (as a tail call standing in for
 * the system call) or called directly.  If cancellation was disabled in
 * the meantime the "system call" fails with ECANCELED instead. */
hidden long __cancel(void)
{
	struct pthread *self = __self();
	if (self->cancel_disable) return -ECANCELED;
	__pthread_exit_internal(PTHREAD_CANCELED);
	return -ECANCELED;
}

hidden long __syscall_cp_c(long nr, long a, long b, long c, long d, long e, long f)
{
	struct pthread *self;
	if (!__libc.can_cancel || (self = __self())->cancel_disable)
		return __syscall6(nr, a, b, c, d, e, f);
	long r = __syscall_cp_asm(&self->cancel, nr, a, b, c, d, e, f);
	if (r == -EINTR && nr != SYS_close && self->cancel && !self->cancel_disable)
		__cancel();
	return r;
}

static void cancel_handler(int sig, siginfo_t *si, void *ctx)
{
	struct pthread *self = __self();
	ucontext_t *uc = ctx;
	uintptr_t pc = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];

	a_barrier();
	if (!self->cancel || self->cancel_disable) return;
	if (self->cancel_async) {
		__restore_sigs(&uc->uc_sigmask);
		__cancel();
		return;
	}
	if (pc >= (uintptr_t)__cp_begin && pc < (uintptr_t)__cp_end)
		uc->uc_mcontext.gregs[REG_RIP] = (greg_t)(uintptr_t)__cp_cancel;
}

static void init_cancellation(void)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
	sa.sa_sigaction = cancel_handler;
	sa.sa_mask.__bits[0] = ~0UL;
	__libc_sigaction(SIGCANCEL, &sa, 0);
}

int pthread_cancel(pthread_t th)
{
	struct pthread *t = (struct pthread *)th;
	if (!__libc.can_cancel) {
		init_cancellation();
		__libc.can_cancel = 1;
	}
	a_store(&t->cancel, 1);
	if (t == __self()) {
		if (!t->cancel_disable && t->cancel_async) __cancel();
		return 0;
	}
	return pthread_kill(th, SIGCANCEL);
}

void pthread_testcancel(void)
{
	struct pthread *self = __self();
	if (self->cancel && !self->cancel_disable) __cancel();
}

hidden void __testcancel(void)
{
	pthread_testcancel();
}

int pthread_setcancelstate(int new, int *old)
{
	if ((unsigned)new > PTHREAD_CANCEL_DISABLE) return EINVAL;
	struct pthread *self = __self();
	if (old) *old = self->cancel_disable;
	self->cancel_disable = (unsigned char)new;
	return 0;
}

int pthread_setcanceltype(int new, int *old)
{
	if ((unsigned)new > PTHREAD_CANCEL_ASYNCHRONOUS) return EINVAL;
	struct pthread *self = __self();
	if (old) *old = self->cancel_async;
	self->cancel_async = (unsigned char)new;
	if (new) pthread_testcancel();
	return 0;
}

void __spfxd_cleanup_push(struct __spfxd_cleanup *cb, void (*f)(void *), void *arg)
{
	struct pthread *self = __self();
	cb->__fn = f;
	cb->__arg = arg;
	cb->__next = self->cleanup;
	self->cleanup = cb;
}

void __spfxd_cleanup_pop(struct __spfxd_cleanup *cb, int run)
{
	__self()->cleanup = cb->__next;
	if (run) cb->__fn(cb->__arg);
}
