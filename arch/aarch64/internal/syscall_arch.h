/*
 * lib-spfxd — AArch64 Linux system call primitives.
 *
 * Kernel ABI (AArch64):
 *   number    -> x8
 *   arguments -> x0 .. x5
 *   result    -> x0; values in [-4095, -1] are negated errno codes
 *   `svc #0` preserves every register except x0.
 */
#ifndef _SPFXD_SYSCALL_ARCH_H
#define _SPFXD_SYSCALL_ARCH_H

#define __SC_ASM(...) __asm__ __volatile__ ("svc 0" : "=r"(x0) : __VA_ARGS__ : "memory", "cc")

static __inline long __syscall0(long n)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0");
	__SC_ASM("r"(x8));
	return x0;
}

static __inline long __syscall1(long n, long a)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	__SC_ASM("r"(x8), "0"(x0));
	return x0;
}

static __inline long __syscall2(long n, long a, long b)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	__SC_ASM("r"(x8), "0"(x0), "r"(x1));
	return x0;
}

static __inline long __syscall3(long n, long a, long b, long c)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	__SC_ASM("r"(x8), "0"(x0), "r"(x1), "r"(x2));
	return x0;
}

static __inline long __syscall4(long n, long a, long b, long c, long d)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	__SC_ASM("r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3));
	return x0;
}

static __inline long __syscall5(long n, long a, long b, long c, long d, long e)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x4 __asm__("x4") = e;
	__SC_ASM("r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4));
	return x0;
}

static __inline long __syscall6(long n, long a, long b, long c, long d, long e, long f)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x4 __asm__("x4") = e;
	register long x5 __asm__("x5") = f;
	__SC_ASM("r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5));
	return x0;
}

/* vDSO symbol names and version used to accelerate time queries. */
#define VDSO_CGT_SYM "__kernel_clock_gettime"
#define VDSO_CGR_SYM "__kernel_clock_getres"
#define VDSO_GTOD_SYM "__kernel_gettimeofday"
#define VDSO_VER "LINUX_2.6.39"

#endif
