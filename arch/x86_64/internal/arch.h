/*
 * lib-spfxd — x86-64 architecture hooks used by portable code.
 */
#ifndef _SPFXD_ARCH_H
#define _SPFXD_ARCH_H

/* Thread pointer: %fs base holds the address of the current thread's
 * struct pthread, whose first word points to itself (x86-64 TLS ABI). */
static __inline void *__arch_tp(void)
{
	void *p;
	__asm__ ("mov %%fs:0,%0" : "=r"(p));
	return p;
}

/* TLS variant II: TLS blocks live below the thread pointer. */
#define TLS_VARIANT_2 1
#define TLS_ABOVE_TP 0

/* Stack protector canary offset fixed by the GCC/Clang x86-64 ABI: %fs:0x28. */
#define TCB_CANARY_OFFSET 0x28

#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003

/* CPU spin-wait hint. */
static __inline void __arch_spin(void) { __asm__ __volatile__ ("pause" ::: "memory"); }

/* Deliberate crash for unrecoverable internal corruption: hlt in user mode
 * raises SIGSEGV immediately, without touching memory or the stack. */
static __inline __attribute__((__noreturn__)) void __arch_crash(void)
{
	for (;;) __asm__ __volatile__ ("hlt" ::: "memory");
}

/* Current SSE rounding mode as 0 nearest, 1 down, 2 up, 3 toward zero. */
static __inline int __arch_round_mode(void)
{
	unsigned csr;
	__asm__ __volatile__ ("stmxcsr %0" : "=m"(csr));
	return (int)((csr >> 13) & 3);
}

/* Enter a program's entry point with the given initial stack pointer and
 * no finalizer (rdx = 0), as the kernel would. */
static __inline __attribute__((__noreturn__)) void __arch_jump_to_entry(unsigned long entry, void *sp)
{
	__asm__ __volatile__ ("mov %1,%%rsp\n\txor %%edx,%%edx\n\tjmp *%0" :: "r"(entry), "r"(sp) : "memory");
	__builtin_unreachable();
}

/* Saved program counter in a signal handler's ucontext_t (cancellation). */
#define UC_PC(uc) ((uc)->uc_mcontext.gregs[REG_RIP])

#define ARCH_PAGE_SIZE 4096UL
#define ARCH_CACHE_LINE 64

#endif
