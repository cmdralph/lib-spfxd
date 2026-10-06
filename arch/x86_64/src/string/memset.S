/*
 * lib-spfxd — memset (x86-64, SSE2).
 *
 * The byte is broadcast to a 64-bit register and an XMM register.  Small
 * sizes use two possibly overlapping stores; 16..128 bytes use 2/4/8
 * overlapping 16-byte stores; larger sizes store an unaligned head, run a
 * 64-byte loop of aligned stores and finish with an unaligned 64-byte
 * tail.  From 2 KiB, rep stosb is used.
 */
	.text
	.global memset
	.type memset,@function
	.p2align 5
memset:
	mov %rdi,%rax
	movzbl %sil,%ecx
	movabs $0x0101010101010101,%r8
	imul %rcx,%r8                      /* byte in every lane */
	cmp $16,%rdx
	jb .Lsmall
	movq %r8,%xmm0
	punpcklqdq %xmm0,%xmm0
	cmp $32,%rdx
	ja .Lover32
	movdqu %xmm0,(%rdi)
	movdqu %xmm0,-16(%rdi,%rdx)
	ret

.Lsmall:
	cmp $8,%edx
	jb .Lunder8
	mov %r8,(%rdi)
	mov %r8,-8(%rdi,%rdx)
	ret
.Lunder8:
	cmp $4,%edx
	jb .Lunder4
	mov %r8d,(%rdi)
	mov %r8d,-4(%rdi,%rdx)
	ret
.Lunder4:
	cmp $2,%edx
	jb .Lunder2
	mov %r8w,(%rdi)
	mov %r8w,-2(%rdi,%rdx)
	ret
.Lunder2:
	test %edx,%edx
	jz 1f
	mov %r8b,(%rdi)
1:	ret

.Lover32:
	cmp $64,%rdx
	ja .Lover64
	movdqu %xmm0,(%rdi)
	movdqu %xmm0,16(%rdi)
	movdqu %xmm0,-32(%rdi,%rdx)
	movdqu %xmm0,-16(%rdi,%rdx)
	ret

.Lover64:
	cmp $128,%rdx
	ja .Lbig
	movdqu %xmm0,(%rdi)
	movdqu %xmm0,16(%rdi)
	movdqu %xmm0,32(%rdi)
	movdqu %xmm0,48(%rdi)
	movdqu %xmm0,-64(%rdi,%rdx)
	movdqu %xmm0,-48(%rdi,%rdx)
	movdqu %xmm0,-32(%rdi,%rdx)
	movdqu %xmm0,-16(%rdi,%rdx)
	ret

.Lbig:
	cmp $2048,%rdx
	jae .Lrep
	lea (%rdi,%rdx),%r10               /* end */
	movdqu %xmm0,(%rdi)
	lea 16(%rdi),%r11
	and $-16,%r11                      /* first aligned address after the head */
	.p2align 4
1:	lea 64(%r11),%r9
	cmp %r10,%r9
	jae 2f
	movdqa %xmm0,(%r11)
	movdqa %xmm0,16(%r11)
	movdqa %xmm0,32(%r11)
	movdqa %xmm0,48(%r11)
	mov %r9,%r11
	jmp 1b
2:	movdqu %xmm0,-64(%r10)
	movdqu %xmm0,-48(%r10)
	movdqu %xmm0,-32(%r10)
	movdqu %xmm0,-16(%r10)
	ret

.Lrep:
	mov %rdx,%rcx
	mov %esi,%eax
	mov %rdi,%r9
	rep stosb
	mov %r9,%rax
	ret
	.size memset,.-memset
	.section .note.GNU-stack,"",@progbits
