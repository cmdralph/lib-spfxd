/*
 * lib-spfxd — getcontext / setcontext / swapcontext / makecontext trampoline
 * (AArch64).
 *
 * ucontext_t offsets (see bits/signal.h, checked in makecontext.c):
 *   uc_sigmask             40
 *   uc_mcontext.regs[n]    184 + 8n
 *   uc_mcontext.sp / pc    432 / 440
 *   uc_mcontext.__reserved 464: an FPSIMD record as the kernel writes it
 *                          (magic, size, fpsr, fpcr, v0..v31 from 480)
 *
 * getcontext saves the callee-saved registers (x19-x30, d8-d15 as part of
 * v8-v15), sp, the return address as pc, FPSR/FPCR and the signal mask; a
 * resumed context returns 0 from getcontext.  setcontext also loads x0-x7,
 * through which makecontext passes the first eight arguments.
 */
#define R(n) (184 + 8 * (n))
#define SP 432
#define PC 440
#define FPREC 464
#define V(n) (480 + 16 * (n))
#define SIGMASK 40

	.text
	.global getcontext
	.type getcontext,%function
getcontext:
	str xzr, [x0, #R(0)]          /* x0: resumed getcontext returns 0 */
	stp x19, x20, [x0, #R(19)]
	stp x21, x22, [x0, #R(21)]
	stp x23, x24, [x0, #R(23)]
	stp x25, x26, [x0, #R(25)]
	stp x27, x28, [x0, #R(27)]
	stp x29, x30, [x0, #R(29)]
	mov x2, sp
	str x2, [x0, #SP]
	str x30, [x0, #PC]
	mov w2, #0x8001               /* FPSIMD_MAGIC 0x46508001 */
	movk w2, #0x4650, lsl #16
	mov w3, #528                  /* record size */
	add x4, x0, #FPREC
	stp w2, w3, [x4]
	mrs x2, fpsr
	mrs x3, fpcr
	stp w2, w3, [x4, #8]
	stp q8, q9, [x0, #V(8)]
	stp q10, q11, [x0, #V(10)]
	stp q12, q13, [x0, #V(12)]
	stp q14, q15, [x0, #V(14)]
	/* rt_sigprocmask(SIG_BLOCK, NULL, &uc_sigmask, 8) */
	mov x9, x0
	add x2, x0, #SIGMASK
	mov x0, #0
	mov x1, #0
	mov x3, #8
	mov x8, #135
	svc #0
	cmn x0, #4095
	b.cs 1f
	mov x0, #0
	ret
1:	neg w0, w0
	b __ucontext_fail
	.size getcontext,.-getcontext

	.global setcontext
	.type setcontext,%function
setcontext:
	mov x9, x0
	/* rt_sigprocmask(SIG_SETMASK, &uc_sigmask, NULL, 8) */
	mov x0, #2
	add x1, x9, #SIGMASK
	mov x2, #0
	mov x3, #8
	mov x8, #135
	svc #0
	cmn x0, #4095
	b.cs 1f
	add x4, x9, #FPREC
	ldr w2, [x4, #8]
	msr fpsr, x2
	ldr w3, [x4, #12]
	msr fpcr, x3
	ldp q8, q9, [x9, #V(8)]
	ldp q10, q11, [x9, #V(10)]
	ldp q12, q13, [x9, #V(12)]
	ldp q14, q15, [x9, #V(14)]
	ldp x19, x20, [x9, #R(19)]
	ldp x21, x22, [x9, #R(21)]
	ldp x23, x24, [x9, #R(23)]
	ldp x25, x26, [x9, #R(25)]
	ldp x27, x28, [x9, #R(27)]
	ldp x29, x30, [x9, #R(29)]
	ldr x2, [x9, #SP]
	mov sp, x2
	ldr x16, [x9, #PC]
	ldp x0, x1, [x9, #R(0)]
	ldp x2, x3, [x9, #R(2)]
	ldp x4, x5, [x9, #R(4)]
	ldp x6, x7, [x9, #R(6)]
	br x16
1:	neg w0, w0
	b __ucontext_fail
	.size setcontext,.-setcontext

	.global swapcontext
	.type swapcontext,%function
swapcontext:
	/* save into oucp exactly like getcontext, then make the saved lr and
	 * pc our caller's return address so that resuming oucp returns 0
	 * from swapcontext */
	mov x10, x1
	mov x11, x30
	mov x12, x0
	bl getcontext
	mov x30, x11
	cbnz w0, 1f
	str x11, [x12, #R(30)]
	str x11, [x12, #PC]
	mov x0, x10
	b setcontext
1:	ret
	.size swapcontext,.-swapcontext

	/* entry functions made by makecontext return here: continue with
	 * uc_link (kept in x19) or exit the process if there is none */
	.global __ucontext_trampoline
	.hidden __ucontext_trampoline
	.type __ucontext_trampoline,%function
__ucontext_trampoline:
	mov x0, x19
	cbz x0, 1f
	bl setcontext
1:	mov x0, #0
	bl exit
	udf #0
	.size __ucontext_trampoline,.-__ucontext_trampoline

	.section .note.GNU-stack,"",%progbits
