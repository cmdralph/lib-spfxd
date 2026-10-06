/* lib-spfxd — bits/signal.h (x86-64 machine context; kernel signal-frame ABI) */
#ifndef _BITS_SIGNAL_H
#define _BITS_SIGNAL_H

#define MINSIGSTKSZ 2048
#define SIGSTKSZ    8192

#if defined(__SPFXD_GNU) || defined(__SPFXD_BSD)
enum {
	REG_R8 = 0, REG_R9, REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15,
	REG_RDI, REG_RSI, REG_RBP, REG_RBX, REG_RDX, REG_RAX, REG_RCX, REG_RSP,
	REG_RIP, REG_EFL, REG_CSGSFS, REG_ERR, REG_TRAPNO, REG_OLDMASK, REG_CR2
};
#define REG_R8 REG_R8
#define REG_RIP REG_RIP
#define REG_RSP REG_RSP
#define NGREG 23
#endif

typedef long long greg_t, gregset_t[23];

struct _fpxreg { unsigned short significand[4], exponent, padding[3]; };
struct _xmmreg { unsigned int element[4]; };
struct _fpstate {
	unsigned short cwd, swd, ftw, fop;
	unsigned long long rip, rdp;
	unsigned int mxcsr, mxcr_mask;
	struct _fpxreg _st[8];
	struct _xmmreg _xmm[16];
	unsigned int padding[24];
};
typedef struct _fpstate *fpregset_t;

/* Layout of the kernel's struct sigcontext. */
struct sigcontext {
	unsigned long r8, r9, r10, r11, r12, r13, r14, r15;
	unsigned long rdi, rsi, rbp, rbx, rdx, rax, rcx, rsp, rip, eflags;
	unsigned short cs, gs, fs, __pad0;
	unsigned long err, trapno, oldmask, cr2;
	struct _fpstate *fpstate;
	unsigned long __reserved1[8];
};

typedef struct {
	gregset_t gregs;
	fpregset_t fpregs;
	unsigned long long __reserved1[8];
} mcontext_t;

typedef struct __spfxd_ucontext {
	unsigned long uc_flags;
	struct __spfxd_ucontext *uc_link;
	stack_t uc_stack;
	mcontext_t uc_mcontext;
	sigset_t uc_sigmask;
	unsigned long __fpregs_mem[64];
} ucontext_t;

#endif
