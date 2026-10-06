/*
 * lib-spfxd — strlen (x86-64, SSE2).
 *
 * Only aligned 16-byte loads are used, so no load crosses a page boundary
 * and nothing past the terminator's page is touched.  The first block is
 * masked to ignore bytes before the start; after reaching 64-byte
 * alignment the loop examines 64 bytes per iteration by folding four
 * vectors with pminub (a zero byte survives the minimum).
 */
	.text
	.global strlen
	.type strlen,@function
	.p2align 5
strlen:
	pxor %xmm0,%xmm0
	mov %edi,%ecx
	and $15,%ecx
	mov %rdi,%rax
	and $-16,%rax
	movdqa (%rax),%xmm1
	pcmpeqb %xmm0,%xmm1
	pmovmskb %xmm1,%edx
	shr %cl,%edx
	test %edx,%edx
	jnz .Lhead
	/* 16-byte steps up to 64-byte alignment */
1:	add $16,%rax
	test $63,%al
	jz .Lloop64
	movdqa (%rax),%xmm1
	pcmpeqb %xmm0,%xmm1
	pmovmskb %xmm1,%edx
	test %edx,%edx
	jz 1b
	bsf %edx,%edx
	add %rdx,%rax
	sub %rdi,%rax
	ret

	.p2align 4
.Lloop64:
	movdqa (%rax),%xmm1
	movdqa 16(%rax),%xmm2
	movdqa 32(%rax),%xmm3
	movdqa 48(%rax),%xmm4
	movdqa %xmm1,%xmm5
	pminub %xmm2,%xmm5
	movdqa %xmm3,%xmm6
	pminub %xmm4,%xmm6
	pminub %xmm6,%xmm5
	pcmpeqb %xmm0,%xmm5
	pmovmskb %xmm5,%edx
	test %edx,%edx
	jnz .Lfound64
	add $64,%rax
	jmp .Lloop64

.Lfound64:
	pcmpeqb %xmm0,%xmm1
	pmovmskb %xmm1,%edx
	test %edx,%edx
	jnz 2f
	add $16,%rax
	pcmpeqb %xmm0,%xmm2
	pmovmskb %xmm2,%edx
	test %edx,%edx
	jnz 2f
	add $16,%rax
	pcmpeqb %xmm0,%xmm3
	pmovmskb %xmm3,%edx
	test %edx,%edx
	jnz 2f
	add $16,%rax
	pcmpeqb %xmm0,%xmm4
	pmovmskb %xmm4,%edx
2:	bsf %edx,%edx
	add %rdx,%rax
	sub %rdi,%rax
	ret

.Lhead:
	bsf %edx,%eax
	ret
	.size strlen,.-strlen
	.section .note.GNU-stack,"",@progbits
