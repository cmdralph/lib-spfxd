/*
 * lib-spfxd — setjmp / _setjmp / longjmp / _longjmp (AArch64 AAPCS64).
 *
 * jmp_buf words: 0-9 x19-x28, 10 x29 (fp), 11 x30 (lr), 12 sp,
 *                13-20 d8-d15
 * Only callee-saved state is kept: everything else is dead across a call.
 * The saved lr is where longjmp resumes (setjmp's return address).
 */
	.text
	.global setjmp
	.global _setjmp
	.type setjmp,%function
	.type _setjmp,%function
setjmp:
_setjmp:
	stp x19, x20, [x0, #0]
	stp x21, x22, [x0, #16]
	stp x23, x24, [x0, #32]
	stp x25, x26, [x0, #48]
	stp x27, x28, [x0, #64]
	stp x29, x30, [x0, #80]
	mov x2, sp
	str x2, [x0, #96]
	stp d8, d9, [x0, #104]
	stp d10, d11, [x0, #120]
	stp d12, d13, [x0, #136]
	stp d14, d15, [x0, #152]
	mov x0, #0
	ret
	.size setjmp,.-setjmp
	.size _setjmp,.-_setjmp

/* longjmp(env, val): returns val from the matching setjmp, or 1 if val is 0 */
	.global longjmp
	.global _longjmp
	.type longjmp,%function
	.type _longjmp,%function
longjmp:
_longjmp:
	ldp x19, x20, [x0, #0]
	ldp x21, x22, [x0, #16]
	ldp x23, x24, [x0, #32]
	ldp x25, x26, [x0, #48]
	ldp x27, x28, [x0, #64]
	ldp x29, x30, [x0, #80]
	ldr x2, [x0, #96]
	mov sp, x2
	ldp d8, d9, [x0, #104]
	ldp d10, d11, [x0, #120]
	ldp d12, d13, [x0, #136]
	ldp d14, d15, [x0, #152]
	cmp w1, #0
	csinc w0, w1, wzr, ne
	br x30
	.size longjmp,.-longjmp
	.size _longjmp,.-_longjmp

	.section .note.GNU-stack,"",%progbits
