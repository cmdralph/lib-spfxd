/* lib-spfxd — makecontext (RISC-V 64): the first eight integer arguments
 * go in a0-a7, further ones on the new stack; the entry function returns
 * (via ra) to a trampoline that resumes uc_link, kept in callee-saved s1. */
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <ucontext.h>
#include "libc.h"

hidden void __ucontext_trampoline(void);

hidden int __ucontext_fail(int e)
{
	errno = e;
	return -1;
}

void makecontext(ucontext_t *uc, void (*fn)(void), int argc, ...)
{
	uintptr_t top = (uintptr_t)uc->uc_stack.ss_sp + uc->uc_stack.ss_size;
	int stack_args = argc > 8 ? argc - 8 : 0;
	uintptr_t sp = (top - (uintptr_t)stack_args * 8) & -16UL;
	uint64_t *s = (uint64_t *)sp;
	va_list ap;
	va_start(ap, argc);
	for (int i = 0; i < argc; i++) {
		uint64_t v = va_arg(ap, uint64_t);
		if (i < 8) uc->uc_mcontext.__gregs[10 + i] = v;
		else s[i - 8] = v;
	}
	va_end(ap);
	uc->uc_mcontext.__gregs[0] = (uintptr_t)fn;                      /* pc */
	uc->uc_mcontext.__gregs[1] = (uintptr_t)__ucontext_trampoline;   /* ra */
	uc->uc_mcontext.__gregs[2] = sp;
	uc->uc_mcontext.__gregs[8] = 0;                                  /* s0/fp */
	uc->uc_mcontext.__gregs[9] = (uintptr_t)uc->uc_link;             /* s1 */
}

/* the assembly in ucontext.S hard-codes these offsets */
_Static_assert(__builtin_offsetof(ucontext_t, uc_sigmask) == 40, "uc_sigmask offset");
_Static_assert(__builtin_offsetof(ucontext_t, uc_mcontext.__gregs) == 176, "gregs offset");
_Static_assert(__builtin_offsetof(ucontext_t, uc_mcontext.__fpregs.__d.__f) == 432, "fpregs offset");
_Static_assert(__builtin_offsetof(ucontext_t, uc_mcontext.__fpregs.__d.__fcsr) == 688, "fcsr offset");
