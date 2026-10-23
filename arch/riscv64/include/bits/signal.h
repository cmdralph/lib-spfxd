/* lib-spfxd — bits/signal.h (RISC-V 64 machine context; kernel signal-frame ABI) */
#ifndef _BITS_SIGNAL_H
#define _BITS_SIGNAL_H
#define MINSIGSTKSZ 2048
#define SIGSTKSZ    8192

/* General registers in the kernel's order: pc, ra, sp, gp, tp, t0-t2,
 * s0-s1, a0-a7, s2-s11, t3-t6 (x0 is replaced by the pc). */
typedef unsigned long greg_t, gregset_t[32];

/* FP state: a 528-byte union of the F, D and Q extension layouts, as in
 * the kernel's union __riscv_fp_state.  Linux RV64GC uses the D view. */
struct __riscv_mc_f_ext_state { unsigned int __f[32]; unsigned int __fcsr; };
struct __riscv_mc_d_ext_state { unsigned long long __f[32]; unsigned int __fcsr; };
struct __riscv_mc_q_ext_state {
	unsigned long long __f[64] __attribute__((__aligned__(16)));
	unsigned int __fcsr;
	unsigned int __reserved[3];
};
union __riscv_mc_fp_state {
	struct __riscv_mc_f_ext_state __f;
	struct __riscv_mc_d_ext_state __d;
	struct __riscv_mc_q_ext_state __q;
};
typedef union __riscv_mc_fp_state fpregset_t;

typedef struct {
	gregset_t __gregs;
	union __riscv_mc_fp_state __fpregs;
} mcontext_t;

#if defined(__SPFXD_GNU) || defined(__SPFXD_BSD)
#define REG_PC 0
#define REG_RA 1
#define REG_SP 2
#define REG_TP 4
#define REG_S0 8
#define REG_S1 9
#define REG_A0 10
#define REG_S2 18
#define REG_NARGS 8
#endif

/* Matches the kernel's struct ucontext: the 1024-bit sigset_t fills the
 * 8-byte kernel mask plus its reserved space, so uc_mcontext lands at the
 * kernel's offset (176, 16-byte aligned). */
typedef struct __spfxd_ucontext {
	unsigned long uc_flags;
	struct __spfxd_ucontext *uc_link;
	stack_t uc_stack;
	sigset_t uc_sigmask;
	mcontext_t uc_mcontext;
} ucontext_t;
#endif
