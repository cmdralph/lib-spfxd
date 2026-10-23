/*
 * lib-spfxd — RISC-V 64 architecture hooks used by portable code.
 */
#ifndef _SPFXD_ARCH_H
#define _SPFXD_ARCH_H

/* Thread pointer: the tp register.  RISC-V uses TLS variant I with no TCB
 * gap: tp points at the first TLS block (the executable's); lib-spfxd
 * keeps its struct pthread immediately below tp. */
static __inline void *__arch_tp(void)
{
	void *p;
	__asm__ ("mv %0, tp" : "=r"(p));
	return p;
}

#define TLS_VARIANT_2 0
#define TLS_ABOVE_TP 1
#define TLS_TCB_SIZE 0
/* dtv-relative TLS offsets (R_RISCV_TLS_DTPREL64) are biased by -0x800
 * so 12-bit immediates reach more of a block; __tls_get_addr adds it */
#define TLS_DTV_OFFSET 0x800

/* Signal handlers return through the kernel's vDSO trampoline: there is
 * no SA_RESTORER and the kernel struct sigaction has no restorer field. */
#define ARCH_SIGRETURN_VDSO 1

/* CPU spin-wait hint: Zihintpause `pause` (a FENCE hint that older cores
 * execute as a no-op). */
static __inline void __arch_spin(void) { __asm__ __volatile__ (".4byte 0x0100000f" ::: "memory"); }

/* Deliberate crash for unrecoverable internal corruption: an illegal
 * instruction raises SIGILL without touching memory. */
static __inline __attribute__((__noreturn__)) void __arch_crash(void)
{
	for (;;) __asm__ __volatile__ ("unimp" ::: "memory");
}

/* Current rounding mode as 0 nearest, 1 down, 2 up, 3 toward zero
 * (frm encodes RNE, RTZ, RDN, RUP, RMM; RMM counts as nearest). */
static __inline int __arch_round_mode(void)
{
	unsigned long frm;
	__asm__ __volatile__ ("frrm %0" : "=r"(frm));
	static const unsigned char map[8] = { 0, 3, 1, 2, 0, 0, 0, 0 };
	return map[frm & 7];
}

/* Enter a program's entry point with the given initial stack pointer and
 * no finalizer (a0 = 0), as the kernel would. */
static __inline __attribute__((__noreturn__)) void __arch_jump_to_entry(unsigned long entry, void *sp)
{
	__asm__ __volatile__ ("mv sp, %1\n\tli a0, 0\n\tjr %0" :: "r"(entry), "r"(sp) : "a0", "memory");
	__builtin_unreachable();
}

/* Saved program counter in a signal handler's ucontext_t (cancellation). */
#define UC_PC(uc) ((uc)->uc_mcontext.__gregs[0])

/* Fallback only: the real page size comes from AT_PAGESZ. */
#define ARCH_PAGE_SIZE 4096UL
#define ARCH_CACHE_LINE 64

#endif
