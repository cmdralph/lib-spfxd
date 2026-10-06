/*
 * lib-spfxd — strcmp (x86-64, SSE2).
 *
 * Compares 16 bytes per step with unaligned loads at the same offset in
 * both strings.  A step is only taken when neither 16-byte window crosses
 * a page boundary; otherwise a single byte is compared, which advances
 * the offset past the boundary within at most 15 steps.  The first
 * position that differs or holds the terminator decides the result.
 */
	.text
	.global strcmp
	.type strcmp,@function
	.p2align 5
strcmp:
	pxor %xmm0,%xmm0
	xor %ecx,%ecx
	.p2align 4
1:	lea (%rdi,%rcx),%r8
	lea (%rsi,%rcx),%r9
	mov %r8d,%eax
	and $4095,%eax
	cmp $4080,%eax
	ja .Lbyte
	mov %r9d,%eax
	and $4095,%eax
	cmp $4080,%eax
	ja .Lbyte
	movdqu (%r8),%xmm1
	movdqu (%r9),%xmm2
	pcmpeqb %xmm1,%xmm2                /* 0xff where equal */
	pcmpeqb %xmm0,%xmm1                /* 0xff where s1 has a zero */
	pandn %xmm2,%xmm1                  /* equal and not terminator */
	pmovmskb %xmm1,%edx
	cmp $0xffff,%edx
	jne .Ldiff
	add $16,%rcx
	jmp 1b
.Ldiff:
	not %edx
	bsf %edx,%edx
	movzbl (%r8,%rdx),%eax
	movzbl (%r9,%rdx),%edx
	sub %edx,%eax
	ret
.Lbyte:
	movzbl (%r8),%eax
	movzbl (%r9),%edx
	sub %edx,%eax
	jnz 2f
	test %edx,%edx
	jz 2f
	inc %rcx
	jmp 1b
2:	ret
	.size strcmp,.-strcmp
	.section .note.GNU-stack,"",@progbits
