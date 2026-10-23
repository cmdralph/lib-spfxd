/*
 * lib-spfxd — RISC-V 64 Linux system call primitives.
 *
 * Kernel ABI (RISC-V):
 *   number    -> a7
 *   arguments -> a0 .. a5
 *   result    -> a0; values in [-4095, -1] are negated errno codes
 *   `ecall` preserves every register except a0.
 */
#ifndef _SPFXD_SYSCALL_ARCH_H
#define _SPFXD_SYSCALL_ARCH_H

#define __SC_ASM(...) __asm__ __volatile__ ("ecall" : "=r"(a0) : __VA_ARGS__ : "memory")

static __inline long __syscall0(long n)
{
	register long a7 __asm__("a7") = n;
	register long a0 __asm__("a0");
	__SC_ASM("r"(a7));
	return a0;
}

static __inline long __syscall1(long n, long a)
{
	register long a7 __asm__("a7") = n;
	register long a0 __asm__("a0") = a;
	__SC_ASM("r"(a7), "0"(a0));
	return a0;
}

static __inline long __syscall2(long n, long a, long b)
{
	register long a7 __asm__("a7") = n;
	register long a0 __asm__("a0") = a;
	register long a1 __asm__("a1") = b;
	__SC_ASM("r"(a7), "0"(a0), "r"(a1));
	return a0;
}

static __inline long __syscall3(long n, long a, long b, long c)
{
	register long a7 __asm__("a7") = n;
	register long a0 __asm__("a0") = a;
	register long a1 __asm__("a1") = b;
	register long a2 __asm__("a2") = c;
	__SC_ASM("r"(a7), "0"(a0), "r"(a1), "r"(a2));
	return a0;
}

static __inline long __syscall4(long n, long a, long b, long c, long d)
{
	register long a7 __asm__("a7") = n;
	register long a0 __asm__("a0") = a;
	register long a1 __asm__("a1") = b;
	register long a2 __asm__("a2") = c;
	register long a3 __asm__("a3") = d;
	__SC_ASM("r"(a7), "0"(a0), "r"(a1), "r"(a2), "r"(a3));
	return a0;
}

static __inline long __syscall5(long n, long a, long b, long c, long d, long e)
{
	register long a7 __asm__("a7") = n;
	register long a0 __asm__("a0") = a;
	register long a1 __asm__("a1") = b;
	register long a2 __asm__("a2") = c;
	register long a3 __asm__("a3") = d;
	register long a4 __asm__("a4") = e;
	__SC_ASM("r"(a7), "0"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4));
	return a0;
}

static __inline long __syscall6(long n, long a, long b, long c, long d, long e, long f)
{
	register long a7 __asm__("a7") = n;
	register long a0 __asm__("a0") = a;
	register long a1 __asm__("a1") = b;
	register long a2 __asm__("a2") = c;
	register long a3 __asm__("a3") = d;
	register long a4 __asm__("a4") = e;
	register long a5 __asm__("a5") = f;
	__SC_ASM("r"(a7), "0"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5));
	return a0;
}

/* vDSO symbol names and version used to accelerate time queries. */
#define VDSO_CGT_SYM "__vdso_clock_gettime"
#define VDSO_CGR_SYM "__vdso_clock_getres"
#define VDSO_GTOD_SYM "__vdso_gettimeofday"
#define VDSO_VER "LINUX_4.15"

#endif
