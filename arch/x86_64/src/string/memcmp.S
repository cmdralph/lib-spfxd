/*
 * lib-spfxd — memcmp (x86-64, SSE2).
 *
 * 16 bytes per step while at least 16 remain; a final step re-compares the
 * last 16 bytes (overlapping) when n >= 16.  Below 16 bytes, 8- and
 * 4-byte loads are compared after byte swapping so an unsigned integer
 * comparison orders them like the bytes.  Returns the difference of the
 * first differing bytes (as unsigned char) or its sign.
 */
	.text
	.global memcmp
	.type memcmp,@function
	.p2align 5
memcmp:
	cmp $16,%rdx
	jb .Lsmall
	xor %ecx,%ecx
	lea -16(%rdx),%r10                 /* offset of the final window */
	.p2align 4
1:	movdqu (%rdi,%rcx),%xmm1
	movdqu (%rsi,%rcx),%xmm2
	pcmpeqb %xmm2,%xmm1
	pmovmskb %xmm1,%eax
	cmp $0xffff,%eax
	jne .Ldiff
	add $16,%rcx
	cmp %r10,%rcx
	jb 1b
	/* last (possibly overlapping) window */
	mov %r10,%rcx
	movdqu (%rdi,%rcx),%xmm1
	movdqu (%rsi,%rcx),%xmm2
	pcmpeqb %xmm2,%xmm1
	pmovmskb %xmm1,%eax
	cmp $0xffff,%eax
	jne .Ldiff
	xor %eax,%eax
	ret
.Ldiff:
	not %eax
	bsf %eax,%eax
	add %rax,%rcx
	movzbl (%rdi,%rcx),%eax
	movzbl (%rsi,%rcx),%edx
	sub %edx,%eax
	ret

.Lsmall:
	cmp $8,%edx
	jb .Lunder8
	/* 8..15: first 8 and last 8 bytes */
	mov (%rdi),%rax
	mov (%rsi),%rcx
	cmp %rcx,%rax
	jne .Lcmp8
	mov -8(%rdi,%rdx),%rax
	mov -8(%rsi,%rdx),%rcx
	cmp %rcx,%rax
	jne .Lcmp8
	xor %eax,%eax
	ret
.Lcmp8:
	bswap %rax
	bswap %rcx
	cmp %rcx,%rax
	sbb %eax,%eax
	or $1,%eax
	ret
.Lunder8:
	cmp $4,%edx
	jb .Lunder4
	mov (%rdi),%eax
	mov (%rsi),%ecx
	cmp %ecx,%eax
	jne .Lcmp4
	mov -4(%rdi,%rdx),%eax
	mov -4(%rsi,%rdx),%ecx
	cmp %ecx,%eax
	jne .Lcmp4
	xor %eax,%eax
	ret
.Lcmp4:
	bswap %eax
	bswap %ecx
	cmp %ecx,%eax
	sbb %eax,%eax
	or $1,%eax
	ret
.Lunder4:
	xor %eax,%eax
	test %edx,%edx
	jz 3f
2:	movzbl (%rdi),%eax
	movzbl (%rsi),%ecx
	sub %ecx,%eax
	jnz 3f
	inc %rdi
	inc %rsi
	dec %edx
	jnz 2b
3:	ret
	.size memcmp,.-memcmp
	.section .note.GNU-stack,"",@progbits
