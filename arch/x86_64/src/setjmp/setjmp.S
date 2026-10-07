/*
 * lib-spfxd — setjmp / _setjmp / longjmp / _longjmp (x86-64 SysV).
 *
 * jmp_buf words: 0 rbx, 1 rbp, 2 r12, 3 r13, 4 r14, 5 r15, 6 rsp, 7 rip
 * Only the callee-saved registers need saving: everything else is dead
 * across a call by definition of the ABI.  The stored rsp is the value the
 * caller will have after setjmp returns (rsp+8, past the return address).
 */
	.text
	.global setjmp
	.global _setjmp
	.type setjmp,@function
	.type _setjmp,@function
setjmp:
_setjmp:
	mov %rbx,0(%rdi)
	mov %rbp,8(%rdi)
	mov %r12,16(%rdi)
	mov %r13,24(%rdi)
	mov %r14,32(%rdi)
	mov %r15,40(%rdi)
	lea 8(%rsp),%rdx
	mov %rdx,48(%rdi)
	mov (%rsp),%rdx
	mov %rdx,56(%rdi)
	xor %eax,%eax
	ret
	.size setjmp,.-setjmp
	.size _setjmp,.-_setjmp

/* longjmp(env, val): returns val from the matching setjmp, or 1 if val
 * is 0.  cmp sets CF exactly when val == 0, so adc yields val or 1. */
	.global longjmp
	.global _longjmp
	.type longjmp,@function
	.type _longjmp,@function
longjmp:
_longjmp:
	xor %eax,%eax
	cmp $1,%esi
	adc %esi,%eax
	mov 0(%rdi),%rbx
	mov 8(%rdi),%rbp
	mov 16(%rdi),%r12
	mov 24(%rdi),%r13
	mov 32(%rdi),%r14
	mov 40(%rdi),%r15
	mov 48(%rdi),%rsp
	jmp *56(%rdi)
	.size longjmp,.-longjmp
	.size _longjmp,.-_longjmp

	.section .note.GNU-stack,"",@progbits
