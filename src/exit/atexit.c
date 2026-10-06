/*
 * lib-spfxd — atexit / __cxa_atexit / __cxa_finalize.
 *
 * Handlers are stored in blocks of FN_PER_BLOCK entries.  The first block is
 * static, guaranteeing the 32 registrations ISO C requires without any
 * allocation; further blocks are mapped directly (not malloc'd), so atexit
 * never drags the allocator into a program and keeps working when the heap
 * is exhausted or corrupt.  Handlers run in reverse order of registration;
 * a handler may itself register more handlers, which then run next.
 */
#include <stdlib.h>
#include <sys/mman.h>
#include "libc.h"
#include "lock.h"

#define FN_PER_BLOCK 32

struct fn_block {
	struct fn_block *next;
	int used;
	int cap;
	struct { void (*f)(void *); void *arg; void *dso; } e[];
};

static struct {
	struct fn_block hdr;
	char storage[FN_PER_BLOCK * 3 * sizeof(void *)];
} builtin = { { 0, 0, FN_PER_BLOCK }, { 0 } };

static struct fn_block *head;
static volatile int lock;
static unsigned generation;  /* bumped by every registration */

int __cxa_atexit(void (*func)(void *), void *arg, void *dso)
{
	__lock_always(&lock);
	if (!head) head = &builtin.hdr;
	if (head->used == head->cap) {
		long r = __syscall(SYS_mmap, 0, 4096, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (__is_err(r)) {
			__unlock_always(&lock);
			return -1;
		}
		struct fn_block *b = (struct fn_block *)r;
		b->cap = (int)((4096 - sizeof *b) / (3 * sizeof(void *)));
		b->used = 0;
		b->next = head;
		head = b;
	}
	head->e[head->used].f = func;
	head->e[head->used].arg = arg;
	head->e[head->used].dso = dso;
	head->used++;
	generation++;
	__unlock_always(&lock);
	return 0;
}

static void call_plain(void *p)
{
	((void (*)(void))p)();
}

int atexit(void (*func)(void))
{
	return __cxa_atexit(call_plain, (void *)func, 0);
}

/* Run (and retire) handlers belonging to dso, or all handlers if dso is 0.
 * The lock is dropped around each call so handlers may call atexit. */
void __cxa_finalize(void *dso)
{
	__lock_always(&lock);
	for (struct fn_block *b = head; b; b = b->next) {
		for (int i = b->used; i-- > 0; ) {
			void (*f)(void *) = b->e[i].f;
			if (!f || (dso && b->e[i].dso != dso)) continue;
			void *arg = b->e[i].arg;
			unsigned gen = generation;
			b->e[i].f = 0;
			__unlock_always(&lock);
			f(arg);
			__lock_always(&lock);
			/* A handler registered new handlers: they run next. */
			if (gen != generation) {
				b = head;
				i = b->used;
			}
		}
		if (!b) break;
	}
	__unlock_always(&lock);
}

hidden void __funcs_on_exit(void)
{
	__cxa_finalize(0);
}
