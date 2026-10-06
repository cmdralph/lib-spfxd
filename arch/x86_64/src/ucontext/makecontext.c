/* lib-spfxd — makecontext (x86-64): the first six integer arguments go in
 * registers, further ones on the new stack; the entry function returns to
 * a trampoline that resumes uc_link. */
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
	static const int regs[6] = { REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9 };
	uintptr_t top = (uintptr_t)uc->uc_stack.ss_sp + uc->uc_stack.ss_size;
	int stack_args = argc > 6 ? argc - 6 : 0;
	uintptr_t sp = (top - (uintptr_t)stack_args * 8) & -16UL;
	sp -= 8;                                   /* return address slot */
	uint64_t *s = (uint64_t *)sp;
	va_list ap;
	va_start(ap, argc);
	for (int i = 0; i < argc; i++) {
		uint64_t v = va_arg(ap, uint64_t);
		if (i < 6) uc->uc_mcontext.gregs[regs[i]] = (greg_t)v;
		else s[1 + i - 6] = v;
	}
	va_end(ap);
	s[0] = (uint64_t)(uintptr_t)__ucontext_trampoline;
	uc->uc_mcontext.gregs[REG_RSP] = (greg_t)sp;
	uc->uc_mcontext.gregs[REG_RIP] = (greg_t)(uintptr_t)fn;
	uc->uc_mcontext.gregs[REG_RBX] = (greg_t)(uintptr_t)uc->uc_link;
}

/* the assembly in ucontext.S hard-codes these offsets */
_Static_assert(__builtin_offsetof(ucontext_t, uc_mcontext) == 40, "uc_mcontext offset");
_Static_assert(__builtin_offsetof(ucontext_t, uc_mcontext.fpregs) == 224, "fpregs offset");
_Static_assert(__builtin_offsetof(ucontext_t, uc_sigmask) == 296, "uc_sigmask offset");
_Static_assert(__builtin_offsetof(ucontext_t, __fpregs_mem) == 424, "fpregs_mem offset");
