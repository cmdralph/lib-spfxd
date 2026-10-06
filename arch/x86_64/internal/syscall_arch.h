/*
 * lib-spfxd — x86-64 Linux system call primitives.
 *
 * Kernel ABI (x86-64):
 *   number    -> rax
 *   arguments -> rdi, rsi, rdx, r10, r8, r9   (r10 instead of rcx: the
 *                `syscall` instruction overwrites rcx with the return rip)
 *   result    -> rax; values in [-4095, -1] are negated errno codes
 *   clobbered -> rcx (return address), r11 (saved rflags); every other
 *                register, including the stack, is preserved
 *
 * The "memory" clobber is required because the kernel may read or write any
 * buffer passed by address, and it prevents the compiler from moving loads
 * and stores across the call.  These helpers return the raw kernel value;
 * errno translation happens in __syscall_ret().
 */
#ifndef _SPFXD_SYSCALL_ARCH_H
#define _SPFXD_SYSCALL_ARCH_H

static __inline long __syscall0(long n)
{
	unsigned long r;
	__asm__ __volatile__ ("syscall" : "=a"(r) : "a"(n) : "rcx", "r11", "memory");
	return (long)r;
}

static __inline long __syscall1(long n, long a1)
{
	unsigned long r;
	__asm__ __volatile__ ("syscall" : "=a"(r) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
	return (long)r;
}

static __inline long __syscall2(long n, long a1, long a2)
{
	unsigned long r;
	__asm__ __volatile__ ("syscall" : "=a"(r) : "a"(n), "D"(a1), "S"(a2)
		: "rcx", "r11", "memory");
	return (long)r;
}

static __inline long __syscall3(long n, long a1, long a2, long a3)
{
	unsigned long r;
	__asm__ __volatile__ ("syscall" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3)
		: "rcx", "r11", "memory");
	return (long)r;
}

static __inline long __syscall4(long n, long a1, long a2, long a3, long a4)
{
	unsigned long r;
	register long r10 __asm__("r10") = a4;
	__asm__ __volatile__ ("syscall" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
		: "rcx", "r11", "memory");
	return (long)r;
}

static __inline long __syscall5(long n, long a1, long a2, long a3, long a4, long a5)
{
	unsigned long r;
	register long r10 __asm__("r10") = a4;
	register long r8 __asm__("r8") = a5;
	__asm__ __volatile__ ("syscall" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
		: "rcx", "r11", "memory");
	return (long)r;
}

static __inline long __syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
	unsigned long r;
	register long r10 __asm__("r10") = a4;
	register long r8 __asm__("r8") = a5;
	register long r9 __asm__("r9") = a6;
	__asm__ __volatile__ ("syscall" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10),
		"r"(r8), "r"(r9) : "rcx", "r11", "memory");
	return (long)r;
}

/* vDSO symbol names and version used to accelerate time queries. */
#define VDSO_CGT_SYM "__vdso_clock_gettime"
#define VDSO_CGR_SYM "__vdso_clock_getres"
#define VDSO_GTOD_SYM "__vdso_gettimeofday"
#define VDSO_TIME_SYM "__vdso_time"
#define VDSO_GETCPU_SYM "__vdso_getcpu"
#define VDSO_VER "LINUX_2.6"

#endif
