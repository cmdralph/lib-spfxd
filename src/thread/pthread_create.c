/*
 * lib-spfxd — thread creation and termination.
 *
 * Each thread's stack, static TLS blocks and TCB come from one mapping:
 *
 *   [guard][stack -> grows down          ][dtv][TLS][struct pthread]
 *
 * The thread is created with clone(CLONE_VM|CLONE_THREAD|...), with
 * CLONE_SETTLS installing the TCB as its thread pointer,
 * CLONE_PARENT_SETTID storing its kernel tid in the TCB before clone
 * returns, and CLONE_CHILD_CLEARTID making the kernel zero that tid and
 * futex-wake it when the thread dies, which is what pthread_join waits on.
 *
 * Exit protocol (detach_state):
 *   JOINABLE --exit--> EXITING   the joiner unmaps the thread after the
 *                                kernel clears tid
 *   DETACHED --exit-->           the thread unmaps its own stack in asm
 *                                (__unmapself) and exits without touching
 *                                memory
 * pthread_detach of an already-exiting thread reaps it immediately.
 */
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include "pthread_impl.h"

static void dummy_tcache(struct pthread *self) { }
weak_alias(dummy_tcache, __malloc_thread_exit);
static void dummy_void(void) { }
weak_alias(dummy_void, __pthread_tsd_run_dtors);
weak_alias(dummy_tcache, __tls_thread_exit);

static int start(void *arg)
{
	struct pthread *self = arg;
	__restore_sigs(&self->sigmask);
	pthread_exit(self->start(self->start_arg));
	return 0;
}

/*
 * Stack cache.  Creating a thread costs an mmap and an mprotect, joining
 * it an munmap; a short-lived thread spends most of its life in those
 * calls.  The mappings of up to STACK_CACHE joined threads are kept and
 * reused for a new thread with the same stack and guard size.  Their
 * TLS/TCB region is cleared before reuse, so a reused thread starts
 * exactly as on a fresh mapping (only stale stack contents remain, as
 * with any stack).  Detached threads unmap their own stack and are never
 * cached.
 */
#define STACK_CACHE 4
static struct { unsigned char *map; size_t size, guard; } stack_cache[STACK_CACHE];
static volatile int stack_cache_lock;

static unsigned char *stack_cache_get(size_t size, size_t guard)
{
	unsigned char *m = 0;
	LOCK(&stack_cache_lock);
	for (int i = 0; i < STACK_CACHE; i++) {
		if (stack_cache[i].map && stack_cache[i].size == size && stack_cache[i].guard == guard) {
			m = stack_cache[i].map;
			stack_cache[i].map = 0;
			break;
		}
	}
	UNLOCK(&stack_cache_lock);
	return m;
}

static int stack_cache_put(unsigned char *map, size_t size, size_t guard)
{
	int done = 0;
	LOCK(&stack_cache_lock);
	for (int i = 0; i < STACK_CACHE && !done; i++) {
		if (!stack_cache[i].map) {
			stack_cache[i].map = map;
			stack_cache[i].size = size;
			stack_cache[i].guard = guard;
			done = 1;
		}
	}
	UNLOCK(&stack_cache_lock);
	return done;
}

int pthread_create(pthread_t *restrict res, const pthread_attr_t *restrict attrp,
	void *(*entry)(void *), void *restrict arg)
{
	struct pthread *self = __self(), *td;
	size_t page = __libc.page_size;
	size_t stack_size, guard, map_size, tls_size = __libc.tls_size;
	unsigned char *map = 0, *stack_top, *tls_mem;
	pthread_attr_t attr;
	sigset_t set;

	if (attrp) {
		attr = *attrp;
	} else {
		memset(&attr, 0, sizeof attr);
		attr.__guardsize = __libc.default_guard;
	}
	stack_size = attr.__stacksize ? attr.__stacksize : __libc.default_stack;

	if (attr.__stackaddr) {
		/* Caller-provided stack: keep TLS and the TCB in our own small
		 * mapping so the caller's stack size is fully usable. */
		map_size = (tls_size + page - 1) & -page;
		map = mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (map == MAP_FAILED) return EAGAIN;
		tls_mem = map;
		stack_top = (unsigned char *)attr.__stackaddr + attr.__stacksize;
		guard = 0;
	} else {
		guard = (attr.__guardsize + page - 1) & -page;
		stack_size = (stack_size + page - 1) & -page;
		if (stack_size > SIZE_MAX / 2 || tls_size > SIZE_MAX / 4) return EINVAL;
		map_size = guard + stack_size + ((tls_size + page - 1) & -page);
		size_t tls_pages = (tls_size + page - 1) & -page;
		if ((map = stack_cache_get(map_size, guard))) {
			memset(map + map_size - tls_pages, 0, tls_pages);
		} else {
			map = mmap(0, map_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
			if (map == MAP_FAILED) return EAGAIN;
			if (mprotect(map + guard, map_size - guard, PROT_READ | PROT_WRITE)) {
				munmap(map, map_size);
				return EAGAIN;
			}
		}
		tls_mem = map + map_size - tls_size;
		tls_mem = (unsigned char *)((uintptr_t)tls_mem & -(uintptr_t)16);
		stack_top = tls_mem;
	}

	td = __copy_tls(tls_mem);
	td->map_base = map;
	td->map_size = map_size;
	td->stack = (void *)((uintptr_t)stack_top & -(uintptr_t)16);
	td->stack_size = attr.__stackaddr ? attr.__stacksize : stack_size;
	td->guard_size = guard;
	td->stack_cacheable = !attr.__stackaddr;
	td->start = entry;
	td->start_arg = arg;
	td->detach_state = attr.__detach ? DT_DETACHED : DT_JOINABLE;
	td->canary = self->canary;
	td->locale = 0;

	/* The new thread starts with application signals blocked and
	 * installs the creator's mask itself, so no handler can run on a
	 * half-initialized thread. */
	__block_app_sigs(&set);
	td->sigmask = set;

	if (!__libc.threaded) __libc.threaded = 1;

	__tl_lock();
	int r = __clone(start, td->stack,
		CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD |
		CLONE_SYSVSEM | CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID,
		td, &td->tid, TP_ADJ(td), &td->tid);
	if (r < 0) {
		__tl_unlock();
		__restore_sigs(&set);
		munmap(map, map_size);
		return r == -EPERM ? EPERM : EAGAIN;
	}
	td->next = self->next;
	td->prev = self;
	self->next->prev = td;
	self->next = td;
	__thread_count++;
	__tl_unlock();
	__restore_sigs(&set);

	*res = (pthread_t)td;
	return 0;
}

hidden void __pthread_exit_internal(void *result)
{
	struct pthread *self = __self();
	sigset_t set;

	self->cancel_disable = 1;
	self->cancel_async = 0;
	self->result = result;

	while (self->cleanup) {
		struct __spfxd_cleanup *c = self->cleanup;
		self->cleanup = c->__next;
		c->__fn(c->__arg);
	}
	__pthread_tsd_run_dtors();
	__tls_thread_exit(self);
	__malloc_thread_exit(self);

	__tl_lock();
	if (__thread_count == 1) {
		/* Last thread: the process terminates as if by exit(0). */
		__tl_unlock();
		exit(0);
	}

	/* From here no signal handler may run on this thread: its TCB and
	 * stack are about to disappear. */
	__block_all_sigs(&set);
	self->prev->next = self->next;
	self->next->prev = self->prev;
	self->next = self->prev = self;
	__thread_count--;

	int state = a_cas(&self->detach_state, DT_JOINABLE, DT_EXITING);
	if (state == DT_DETACHED && self->map_base) {
		/* Stop the kernel from writing the clear-tid word into memory
		 * that is about to be unmapped (and possibly reused). */
		__syscall(SYS_set_tid_address, 0);
		__tl_unlock();
		__unmapself(self->map_base, self->map_size);
	}
	__tl_unlock();
	for (;;) __syscall(SYS_exit, 0);
}

_Noreturn void pthread_exit(void *result)
{
	__pthread_exit_internal(result);
	for (;;) __arch_crash();
}

static int join_common(struct pthread *t, void **res, const struct timespec *at, int try)
{
	int tid, r = 0;
	if (t->detach_state == DT_DETACHED) return EINVAL;
	if (t == __self()) return EDEADLK;
	if (!try) pthread_testcancel();
	while ((tid = t->tid)) {
		if (try) return EBUSY;
		r = __timedwait(&t->tid, tid, CLOCK_REALTIME, at, 0, 1);
		if (r == ETIMEDOUT || r == EINVAL) return r;
	}
	if (res) *res = t->result;
	if (t->map_base && !(t->stack_cacheable && stack_cache_put(t->map_base, t->map_size, t->guard_size)))
		munmap(t->map_base, t->map_size);
	return 0;
}

int pthread_join(pthread_t th, void **res)
{
	return join_common((struct pthread *)th, res, 0, 0);
}

int pthread_tryjoin_np(pthread_t th, void **res)
{
	return join_common((struct pthread *)th, res, 0, 1);
}

int pthread_timedjoin_np(pthread_t th, void **res, const struct timespec *at)
{
	return join_common((struct pthread *)th, res, at, 0);
}

int pthread_detach(pthread_t th)
{
	struct pthread *t = (struct pthread *)th;
	int s = a_cas(&t->detach_state, DT_JOINABLE, DT_DETACHED);
	if (s == DT_JOINABLE) return 0;
	if (s == DT_EXITING || s == DT_EXITED) return pthread_join(th, 0);
	return EINVAL;
}

pthread_t pthread_self(void)
{
	return (pthread_t)__self();
}

#undef pthread_equal
int pthread_equal(pthread_t a, pthread_t b)
{
	return a == b;
}

pid_t pthread_gettid_np(pthread_t th)
{
	return ((struct pthread *)th)->tid;
}

pid_t gettid(void)
{
	return __self()->tid;
}
