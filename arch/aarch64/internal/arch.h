/*
 * lib-spfxd — AArch64 architecture hooks used by portable code.
 */
#ifndef _SPFXD_ARCH_H
#define _SPFXD_ARCH_H

/* Thread pointer: TPIDR_EL0.  AArch64 uses TLS variant I: the thread
 * pointer is followed by a 16-byte TCB reserved for the implementation and
 * then the TLS blocks; lib-spfxd keeps its struct pthread immediately
 * below the thread pointer. */
static __inline void *__arch_tp(void)
{
	void *p;
	__asm__ ("mrs %0, tpidr_el0" : "=r"(p));
	return p;
}

#define TLS_VARIANT_2 0
#define TLS_ABOVE_TP 1
#define TLS_TCB_SIZE 16        /* gap between the thread pointer and the first block */

/* CPU spin-wait hint. */
static __inline void __arch_spin(void) { __asm__ __volatile__ ("yield" ::: "memory"); }

/* Deliberate crash for unrecoverable internal corruption: a permanently
 * undefined instruction raises SIGILL without touching memory. */
static __inline __attribute__((__noreturn__)) void __arch_crash(void)
{
	for (;;) __asm__ __volatile__ ("udf #0" ::: "memory");
}

/* Current rounding mode as 0 nearest, 1 down, 2 up, 3 toward zero
 * (FPCR.RMode encodes nearest, up, down, zero). */
static __inline int __arch_round_mode(void)
{
	unsigned long fpcr;
	__asm__ __volatile__ ("mrs %0, fpcr" : "=r"(fpcr));
	static const unsigned char map[4] = { 0, 2, 1, 3 };
	return map[(fpcr >> 22) & 3];
}

/* Enter a program's entry point with the given initial stack pointer and
 * no finalizer (x0 = 0), as the kernel would. */
static __inline __attribute__((__noreturn__)) void __arch_jump_to_entry(unsigned long entry, void *sp)
{
	/* x0 = 0: no rtld_fini for the program to register; x0 is clobbered
	 * so neither input is allocated to it */
	__asm__ __volatile__ ("mov sp, %1\n\tmov x0, #0\n\tbr %0" :: "r"(entry), "r"(sp) : "x0", "memory");
	__builtin_unreachable();
}

/* Saved program counter in a signal handler's ucontext_t (cancellation). */
#define UC_PC(uc) ((uc)->uc_mcontext.pc)

/* Fallback only: the real page size (4, 16 or 64 KiB on AArch64 Linux)
 * comes from AT_PAGESZ. */
#define ARCH_PAGE_SIZE 4096UL
#define ARCH_CACHE_LINE 64

#endif
