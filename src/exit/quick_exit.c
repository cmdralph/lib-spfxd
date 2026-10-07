/* lib-spfxd — at_quick_exit / quick_exit (C11). */
#include <stdlib.h>
#include "libc.h"
#include "lock.h"

#define QUICK_MAX 32
static void (*funcs[QUICK_MAX])(void);
static int count;
static volatile int lock;

int at_quick_exit(void (*func)(void))
{
	int r = -1;
	__lock_always(&lock);
	if (count < QUICK_MAX) {
		funcs[count++] = func;
		r = 0;
	}
	__unlock_always(&lock);
	return r;
}

_Noreturn void quick_exit(int code)
{
	__lock_always(&lock);
	while (count > 0) {
		void (*f)(void) = funcs[--count];
		__unlock_always(&lock);
		f();
		__lock_always(&lock);
	}
	_Exit(code);
}
