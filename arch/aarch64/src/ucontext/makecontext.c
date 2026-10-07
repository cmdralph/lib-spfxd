/* lib-spfxd — makecontext (AArch64): the first eight integer arguments go
 * in x0-x7, further ones on the new stack; the entry function returns (via
 * lr) to a trampoline that resumes uc_link, kept in callee-saved x19. */
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
		if (i < 8) uc->uc_mcontext.regs[i] = v;
		else s[i - 8] = v;
	}
	va_end(ap);
	uc->uc_mcontext.sp = sp;
	uc->uc_mcontext.pc = (uintptr_t)fn;
	uc->uc_mcontext.regs[29] = 0;
	uc->uc_mcontext.regs[30] = (uintptr_t)__ucontext_trampoline;
	uc->uc_mcontext.regs[19] = (uintptr_t)uc->uc_link;
}

/* the assembly in ucontext.S hard-codes these offsets */
_Static_assert(__builtin_offsetof(ucontext_t, uc_sigmask) == 40, "uc_sigmask offset");
_Static_assert(__builtin_offsetof(ucontext_t, uc_mcontext.regs) == 184, "regs offset");
_Static_assert(__builtin_offsetof(ucontext_t, uc_mcontext.sp) == 432, "sp offset");
_Static_assert(__builtin_offsetof(ucontext_t, uc_mcontext.pc) == 440, "pc offset");
_Static_assert(__builtin_offsetof(ucontext_t, uc_mcontext.__reserved) == 464, "reserved offset");
