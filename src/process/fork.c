/*
 * lib-spfxd — fork, _Fork and pthread_atfork.
 *
 * Around the fork system call every internal lock that another thread
 * could hold is taken (allocator, stdio stream list, dynamic linker, the
 * thread list), so the child inherits them in a consistent, unlocked
 * state.  The child is single-threaded: its thread list is reset and its
 * TCB gets the new kernel tid.
 *
 * Handlers: prepare run newest-first, parent/child oldest-first (POSIX).
 */
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "pthread_impl.h"

static void dummy_atfork(int who) { }
weak_alias(dummy_atfork, __malloc_atfork);
weak_alias(dummy_atfork, __stdio_atfork);
weak_alias(dummy_atfork, __ldso_atfork);

struct atfork {
	void (*prepare)(void), (*parent)(void), (*child)(void);
	struct atfork *newer, *older;
};
static struct atfork *newest, *oldest;
static volatile int atfork_lock;

int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
	struct atfork *n = malloc(sizeof *n);
	if (!n) return ENOMEM;
	n->prepare = prepare;
	n->parent = parent;
	n->child = child;
	__lock_always(&atfork_lock);
	n->newer = 0;
	n->older = newest;
	if (newest) newest->newer = n;
	else oldest = n;
	newest = n;
	__unlock_always(&atfork_lock);
	return 0;
}

static void run_handlers(int who)
{
	if (who < 0) {
		for (struct atfork *a = newest; a; a = a->older)
			if (a->prepare) a->prepare();
	} else {
		for (struct atfork *a = oldest; a; a = a->newer) {
			void (*f)(void) = who ? a->child : a->parent;
			if (f) f();
		}
	}
}

/* The fork system call with the library's own bookkeeping, but without
 * running application handlers (POSIX _Fork, async-signal-safe). */
pid_t _Fork(void)
{
	sigset_t set;
	struct pthread *self = __self();

	__block_all_sigs(&set);
	__tl_lock();
	long ret = __syscall(SYS_clone, SIGCHLD, 0, 0, 0, 0);
	if (ret == 0) {
		self->tid = (int)__syscall(SYS_gettid);
		self->next = self->prev = self;
		self->detach_state = DT_JOINABLE;
		__thread_count = 1;
		__thread_list_lock = 0;
		__libc.threaded = 0;
	} else {
		__tl_unlock();
	}
	__restore_sigs(&set);
	return (pid_t)__syscall_ret((unsigned long)ret);
}

pid_t fork(void)
{
	__lock_always(&atfork_lock);
	run_handlers(-1);
	__ldso_atfork(-1);
	__stdio_atfork(-1);
	__malloc_atfork(-1);
	pid_t pid = _Fork();
	int who = pid == 0 ? 1 : 0;
	__malloc_atfork(who);
	__stdio_atfork(who);
	__ldso_atfork(who);
	if (who) atfork_lock = 0;
	else __unlock_always(&atfork_lock);
	run_handlers(who);
	return pid;
}
