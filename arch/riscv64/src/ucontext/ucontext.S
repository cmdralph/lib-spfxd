/*
 * lib-spfxd — getcontext / setcontext / swapcontext / makecontext trampoline
 * (RISC-V 64).
 *
 * ucontext_t offsets (see bits/signal.h, checked in makecontext.c):
 *   uc_sigmask                      40
 *   uc_mcontext.__gregs[n]          176 + 8n  (slot 0 is the pc, slot n
 *                                   the register xn otherwise)
 *   uc_mcontext.__fpregs.__d.__f[n] 432 + 8n,  __fcsr at 688
 *
 * getcontext saves the callee-saved registers (s0-s11, fs0-fs11), ra,
 * sp, gp, the return address as pc, fcsr and the signal mask; a resumed
 * context returns 0 from getcontext.  setcontext also loads a0-a7, through
 * which makecontext passes the first eight arguments.  tp and gp are not
 * reloaded: they belong to the thread and the program, not the context.
 */
	.text
	.global getcontext
	.type getcontext,%function
getcontext:
	sd zero, 256(a0)          /* a0: resumed getcontext returns 0 */
	sd ra, 184(a0)
	sd ra, 176(a0)            /* pc */
	sd sp, 192(a0)
	sd gp, 200(a0)
	sd s0, 240(a0)
	sd s1, 248(a0)
	sd s2, 320(a0)
	sd s3, 328(a0)
	sd s4, 336(a0)
	sd s5, 344(a0)
	sd s6, 352(a0)
	sd s7, 360(a0)
	sd s8, 368(a0)
	sd s9, 376(a0)
	sd s10, 384(a0)
	sd s11, 392(a0)
	fsd fs0, 496(a0)
	fsd fs1, 504(a0)
	fsd fs2, 576(a0)
	fsd fs3, 584(a0)
	fsd fs4, 592(a0)
	fsd fs5, 600(a0)
	fsd fs6, 608(a0)
	fsd fs7, 616(a0)
	fsd fs8, 624(a0)
	fsd fs9, 632(a0)
	fsd fs10, 640(a0)
	fsd fs11, 648(a0)
	frcsr t0
	sw t0, 688(a0)
	/* rt_sigprocmask(SIG_BLOCK, NULL, &uc_sigmask, 8) */
	addi a2, a0, 40
	li a0, 0
	li a1, 0
	li a3, 8
	li a7, 135
	ecall
	li t2, -4095
	bgeu a0, t2, 1f
	li a0, 0
	ret
1:	neg a0, a0
	tail __ucontext_fail
	.size getcontext,.-getcontext

	.global setcontext
	.type setcontext,%function
setcontext:
	mv t1, a0
	/* rt_sigprocmask(SIG_SETMASK, &uc_sigmask, NULL, 8) */
	li a0, 2
	addi a1, t1, 40
	li a2, 0
	li a3, 8
	li a7, 135
	ecall
	li t2, -4095
	bgeu a0, t2, 1f
	lw t0, 688(t1)
	fscsr t0
	fld fs0, 496(t1)
	fld fs1, 504(t1)
	fld fs2, 576(t1)
	fld fs3, 584(t1)
	fld fs4, 592(t1)
	fld fs5, 600(t1)
	fld fs6, 608(t1)
	fld fs7, 616(t1)
	fld fs8, 624(t1)
	fld fs9, 632(t1)
	fld fs10, 640(t1)
	fld fs11, 648(t1)
	ld ra, 184(t1)
	ld sp, 192(t1)
	ld s0, 240(t1)
	ld s1, 248(t1)
	ld s2, 320(t1)
	ld s3, 328(t1)
	ld s4, 336(t1)
	ld s5, 344(t1)
	ld s6, 352(t1)
	ld s7, 360(t1)
	ld s8, 368(t1)
	ld s9, 376(t1)
	ld s10, 384(t1)
	ld s11, 392(t1)
	ld a0, 256(t1)
	ld a1, 264(t1)
	ld a2, 272(t1)
	ld a3, 280(t1)
	ld a4, 288(t1)
	ld a5, 296(t1)
	ld a6, 304(t1)
	ld a7, 312(t1)
	ld t0, 176(t1)
	jr t0
1:	neg a0, a0
	tail __ucontext_fail
	.size setcontext,.-setcontext

	.global swapcontext
	.type swapcontext,%function
swapcontext:
	/* save into oucp exactly like getcontext (which uses only a0-a3, a7
	 * and t0-t2), then make the saved ra and pc our caller's return
	 * address so that resuming oucp returns 0 from swapcontext */
	mv t4, a0
	mv t5, a1
	mv t6, ra
	call getcontext
	mv ra, t6
	bnez a0, 1f
	sd t6, 184(t4)
	sd t6, 176(t4)
	mv a0, t5
	tail setcontext
1:	ret
	.size swapcontext,.-swapcontext

	/* entry functions made by makecontext return here: continue with
	 * uc_link (kept in s1) or exit the process if there is none */
	.global __ucontext_trampoline
	.hidden __ucontext_trampoline
	.type __ucontext_trampoline,%function
__ucontext_trampoline:
	mv a0, s1
	beqz a0, 1f
	call setcontext
1:	li a0, 0
	call exit
	unimp
	.size __ucontext_trampoline,.-__ucontext_trampoline

	.section .note.GNU-stack,"",%progbits
