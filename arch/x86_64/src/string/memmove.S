/*
 * lib-spfxd — memmove (x86-64, SSE2).
 *
 * If dst is below src, or the regions do not overlap, memcpy's code is a
 * correct forward copy (see memcpy.S).  Sizes up to 128 are also handled
 * there since those paths load everything before storing.  Otherwise
 * (dst > src with overlap, n > 128) copy backward: the first 64 bytes and
 * the unaligned last 16 are loaded up front, the body is copied from the
 * end in 64-byte steps with 16-byte-aligned stores, then the saved head and
 * tail are written.
 */
	.text
	.global memmove
	.type memmove,@function
	.p2align 5
memmove:
	mov %rdi,%rcx
	sub %rsi,%rcx
	cmp %rdx,%rcx
	jae __memcpy_fwd                   /* dst - src >= n (unsigned): forward ok */
	cmp $128,%rdx
	jbe __memcpy_fwd
	mov %rdi,%rax
	/* save the first 64 source bytes and the last 16 */
	movdqu (%rsi),%xmm4
	movdqu 16(%rsi),%xmm5
	movdqu 32(%rsi),%xmm6
	movdqu 48(%rsi),%xmm7
	movdqu -16(%rsi,%rdx),%xmm8
	lea (%rdi,%rdx),%r10               /* dst end */
	mov %r10,%r8                       /* original dst end, for the tail */
	lea (%rsi,%rdx),%r9                /* src end */
	/* align the dst end down to 16 */
	mov %r10,%rcx
	and $15,%ecx
	sub %rcx,%r10
	sub %rcx,%r9
	sub %rcx,%rdx
	.p2align 4
1:	cmp $64,%rdx
	jbe 2f
	movdqu -16(%r9),%xmm0
	movdqu -32(%r9),%xmm1
	movdqu -48(%r9),%xmm2
	movdqu -64(%r9),%xmm3
	movdqa %xmm0,-16(%r10)
	movdqa %xmm1,-32(%r10)
	movdqa %xmm2,-48(%r10)
	movdqa %xmm3,-64(%r10)
	sub $64,%r9
	sub $64,%r10
	sub $64,%rdx
	jmp 1b
2:	movdqu %xmm4,(%rdi)
	movdqu %xmm5,16(%rdi)
	movdqu %xmm6,32(%rdi)
	movdqu %xmm7,48(%rdi)
	movdqu %xmm8,-16(%r8)
	ret
	.size memmove,.-memmove
	.section .note.GNU-stack,"",@progbits
