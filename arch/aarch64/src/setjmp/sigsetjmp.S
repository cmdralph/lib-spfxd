/*
 * lib-spfxd — int sigsetjmp(sigjmp_buf env, int savemask)
 *
 * jmp_buf layout: 176 bytes of registers, then the flag word at 176 and
 * the saved signal mask at 184.  The mask is read with rt_sigprocmask
 * directly, so lr and sp are exactly the caller's when we branch into
 * setjmp.  svc preserves every register but x0, so env lives in x9.
 */
	.text
	.global sigsetjmp
	.global __sigsetjmp
	.type sigsetjmp,%function
	.type __sigsetjmp,%function
sigsetjmp:
__sigsetjmp:
	mov w1, w1             /* int: zero-extend */
	str x1, [x0, #176]
	cbz x1, 1f
	mov x9, x0
	add x2, x0, #184       /* oldset */
	mov x1, #0             /* set = NULL */
	mov x0, #0             /* how (ignored) */
	mov x3, #8             /* kernel sigset size */
	mov x8, #135           /* rt_sigprocmask */
	svc #0
	mov x0, x9
1:	b setjmp
	.size sigsetjmp,.-sigsetjmp
	.size __sigsetjmp,.-__sigsetjmp

	.section .note.GNU-stack,"",%progbits
