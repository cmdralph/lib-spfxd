/*
 * lib-spfxd — run a function on every thread (used for set*id).
 *
 * Linux credentials are per thread, but POSIX demands that setuid() and
 * friends affect the whole process.  The caller performs the operation
 * first; if it succeeds, every other thread is sent SIGSYNCCALL and runs
 * the same operation from the signal handler.  The thread list lock keeps
 * the set of threads stable meanwhile.
 */
#include <signal.h>
#include <string.h>
#include "pthread_impl.h"


static void (*sync_fn)(void *);
static void *sync_ctx;
static volatile int sync_done;

static void sync_handler(int sig)
{
	int e = __self()->errno_val;
	sync_fn(sync_ctx);
	a_inc(&sync_done);
	__futex_wake(&sync_done, 1, 1);
	__self()->errno_val = e;
}

hidden void __synccall(void (*fn)(void *), void *ctx)
{
	sigset_t set;
	struct sigaction sa;

	__block_app_sigs(&set);
	__tl_lock();
	struct pthread *self = __self();
	if (self->next == self) {
		__tl_unlock();
		__restore_sigs(&set);
		return;
	}
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = sync_handler;
	sa.sa_flags = SA_RESTART | SA_ONSTACK;
	sa.sa_mask.__bits[0] = ~0UL;
	__libc_sigaction(SIGSYNCCALL, &sa, 0);

	sync_fn = fn;
	sync_ctx = ctx;
	sync_done = 0;
	int count = 0;
	for (struct pthread *t = self->next; t != self; t = t->next) {
		if (t->tid && !__syscall(SYS_tkill, t->tid, SIGSYNCCALL)) count++;
	}
	int d;
	while ((d = a_load(&sync_done)) < count) __futex_wait(&sync_done, d, 1);
	__tl_unlock();
	__restore_sigs(&set);
}
