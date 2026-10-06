/*
 * lib-spfxd — strchrnul (x86-64, SSE2).
 *
 * Each aligned 16-byte block is compared against both the target byte and
 * zero; the first position matching either ends the search.
 */
	.text
	.global strchrnul
	.global __strchrnul
	.hidden __strchrnul
	.type strchrnul,@function
	.type __strchrnul,@function

/* rdi = s, esi = c; returns pointer to first c or terminator in rax */
	.p2align 5
strchrnul:
__strchrnul:
	movd %esi,%xmm1
	punpcklbw %xmm1,%xmm1
	punpcklwd %xmm1,%xmm1
	pshufd $0,%xmm1,%xmm1              /* c in all 16 bytes */
	pxor %xmm0,%xmm0
	mov %edi,%ecx
	and $15,%ecx
	mov %rdi,%rax
	and $-16,%rax
	movdqa (%rax),%xmm2
	movdqa %xmm2,%xmm3
	pcmpeqb %xmm0,%xmm2
	pcmpeqb %xmm1,%xmm3
	por %xmm3,%xmm2
	pmovmskb %xmm2,%edx
	shr %cl,%edx
	shl %cl,%edx
	test %edx,%edx
	jnz 2f
	.p2align 4
1:	add $16,%rax
	movdqa (%rax),%xmm2
	movdqa %xmm2,%xmm3
	pcmpeqb %xmm0,%xmm2
	pcmpeqb %xmm1,%xmm3
	por %xmm3,%xmm2
	pmovmskb %xmm2,%edx
	test %edx,%edx
	jz 1b
2:	bsf %edx,%edx
	add %rdx,%rax
	ret
	.size strchrnul,.-strchrnul
	.size __strchrnul,.-__strchrnul

	.section .note.GNU-stack,"",@progbits
