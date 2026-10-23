/*
 * lib-spfxd — int sigsetjmp(sigjmp_buf env, int savemask)
 *
 * jmp_buf layout: 208 bytes of registers, then the flag word at 208 and
 * the saved signal mask at 216.  The mask is read with rt_sigprocmask
 * directly, so ra and sp are exactly the caller's when we jump into
 * setjmp.  ecall preserves every register but a0, so env lives in t0.
 */
	.text
	.global sigsetjmp
	.global __sigsetjmp
	.type sigsetjmp,%function
	.type __sigsetjmp,%function
sigsetjmp:
__sigsetjmp:
	sd a1, 208(a0)
	beqz a1, 1f
	mv t0, a0
	addi a2, a0, 216       /* oldset */
	li a1, 0               /* set = NULL */
	li a0, 0               /* how (ignored) */
	li a3, 8               /* kernel sigset size */
	li a7, 135             /* rt_sigprocmask */
	ecall
	mv a0, t0
1:	tail setjmp
	.size sigsetjmp,.-sigsetjmp
	.size __sigsetjmp,.-__sigsetjmp

	.section .note.GNU-stack,"",%progbits
