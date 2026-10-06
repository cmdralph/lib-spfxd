/*
 * lib-spfxd — memchr (x86-64, SSE2).
 *
 * Aligned 16-byte blocks; matches before the start are masked off in the
 * first block and a match is accepted only if it lies within n bytes of
 * the start.  Aligned loads never cross a page, so reading the remainder
 * of the last block is safe.
 */
	.text
	.global memchr
	.type memchr,@function
	.p2align 5
memchr:
	test %rdx,%rdx
	jz .Lnull
	movd %esi,%xmm1
	punpcklbw %xmm1,%xmm1
	punpcklwd %xmm1,%xmm1
	pshufd $0,%xmm1,%xmm1
	mov %edi,%ecx
	and $15,%ecx
	mov %rdi,%rax
	and $-16,%rax
	movdqa (%rax),%xmm2
	pcmpeqb %xmm1,%xmm2
	pmovmskb %xmm2,%r8d
	shr %cl,%r8d
	test %r8d,%r8d
	jz 1f
	bsf %r8d,%r8d
	cmp %rdx,%r8
	jae .Lnull
	lea (%rdi,%r8),%rax
	ret
1:	/* bytes covered so far: 16 - misalignment */
	mov $16,%r9d
	sub %ecx,%r9d
	cmp %r9,%rdx
	jbe .Lnull
	sub %r9,%rdx                       /* bytes remaining after this block */
	.p2align 4
2:	add $16,%rax
	movdqa (%rax),%xmm2
	pcmpeqb %xmm1,%xmm2
	pmovmskb %xmm2,%r8d
	test %r8d,%r8d
	jnz 3f
	sub $16,%rdx
	ja 2b
	jmp .Lnull
3:	bsf %r8d,%r8d
	cmp %rdx,%r8
	jae .Lnull
	add %r8,%rax
	ret
.Lnull:
	xor %eax,%eax
	ret
	.size memchr,.-memchr
	.section .note.GNU-stack,"",@progbits
