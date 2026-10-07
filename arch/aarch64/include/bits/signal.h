/* lib-spfxd — bits/signal.h (AArch64 machine context; kernel signal-frame ABI) */
#ifndef _BITS_SIGNAL_H
#define _BITS_SIGNAL_H

#define MINSIGSTKSZ 5120
#define SIGSTKSZ    16384

typedef unsigned long greg_t, gregset_t[34];

/* Saved FP/SIMD state as user code sees it (getcontext/makecontext). */
typedef struct {
	__uint128_t vregs[32];
	unsigned int fpsr, fpcr;
} fpregset_t;

/* Layout of the kernel's struct sigcontext: the general registers, then a
 * 4 KiB area of tagged records (FP/SIMD state, SVE, ...). */
struct sigcontext {
	unsigned long fault_address;
	unsigned long regs[31];
	unsigned long sp, pc, pstate;
	unsigned char __reserved[4096] __attribute__((__aligned__(16)));
};
typedef struct sigcontext mcontext_t;

/* FP/SIMD record in __reserved (magic FPSIMD_MAGIC) */
#if defined(__SPFXD_GNU) || defined(__SPFXD_BSD)
#define FPSIMD_MAGIC 0x46508001
struct _aarch64_ctx { unsigned int magic, size; };
struct fpsimd_context {
	struct _aarch64_ctx head;
	unsigned int fpsr, fpcr;
	__uint128_t vregs[32];
};
#endif

/* Matches the kernel's struct ucontext: with the 1024-bit sigset_t the
 * signal mask fills exactly the space the kernel reserves for it. */
typedef struct __spfxd_ucontext {
	unsigned long uc_flags;
	struct __spfxd_ucontext *uc_link;
	stack_t uc_stack;
	sigset_t uc_sigmask;
	mcontext_t uc_mcontext;
} ucontext_t;

#endif
