/*
 * lib-spfxd — memcmp (x86-64, SSE2 baseline and AVX2).
 *
 * 16 bytes per step while at least 16 remain; a final step re-compares the
 * last 16 bytes (overlapping) when n >= 16.  Below 16 bytes, 8- and
 * 4-byte loads are compared after byte swapping so an unsigned integer
 * comparison orders them like the bytes.  Returns the difference of the
 * first differing bytes (as unsigned char) or its sign.
 *
 * AVX2 (n >= 32): 128 bytes per iteration while that much remains, then
 * 32-byte steps and a final overlapping 32-byte window.  The equality
 * mask plus one is zero when all 32 bytes match; otherwise its lowest
 * set bit is the first mismatch.
 */
#include "cpu.h"
	.text
	.global memcmp
	.type memcmp,@function
	.p2align 5
memcmp:
	cmp $32,%rdx
	jb 0f
	testb $CPU_AVX2,__cpu_features(%rip)
	jnz .Lavx2
0:	cmp $16,%rdx
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

	.p2align 5
.Lavx2:
	xor %ecx,%ecx
	lea -32(%rdx),%r10                 /* offset of the final window */
	cmp $128,%rdx
	jb .La_step
	lea -128(%rdx),%r11                /* last offset with 128 bytes left */
	.p2align 4
.La_loop4:
	vmovdqu (%rdi,%rcx),%ymm1
	vmovdqu 32(%rdi,%rcx),%ymm2
	vmovdqu 64(%rdi,%rcx),%ymm3
	vmovdqu 96(%rdi,%rcx),%ymm4
	vpcmpeqb (%rsi,%rcx),%ymm1,%ymm1
	vpcmpeqb 32(%rsi,%rcx),%ymm2,%ymm2
	vpcmpeqb 64(%rsi,%rcx),%ymm3,%ymm3
	vpcmpeqb 96(%rsi,%rcx),%ymm4,%ymm4
	vpand %ymm1,%ymm2,%ymm5
	vpand %ymm3,%ymm4,%ymm6
	vpand %ymm5,%ymm6,%ymm6
	vpmovmskb %ymm6,%eax
	inc %eax
	jnz .La_find4
	sub $-128,%rcx
	cmp %r11,%rcx
	jbe .La_loop4
.La_step:
	cmp %r10,%rcx
	jae .La_final
	vmovdqu (%rdi,%rcx),%ymm1
	vpcmpeqb (%rsi,%rcx),%ymm1,%ymm1
	vpmovmskb %ymm1,%eax
	inc %eax
	jnz .La_diff
	add $32,%rcx
	jmp .La_step
.La_final:
	mov %r10,%rcx
	vmovdqu (%rdi,%rcx),%ymm1
	vpcmpeqb (%rsi,%rcx),%ymm1,%ymm1
	vpmovmskb %ymm1,%eax
	inc %eax
	jnz .La_diff
	vzeroupper
	ret
.La_find4:
	vpmovmskb %ymm1,%eax
	inc %eax
	jnz .La_diff
	add $32,%rcx
	vpmovmskb %ymm2,%eax
	inc %eax
	jnz .La_diff
	add $32,%rcx
	vpmovmskb %ymm3,%eax
	inc %eax
	jnz .La_diff
	add $32,%rcx
	vpmovmskb %ymm4,%eax
	inc %eax
.La_diff:
	bsf %eax,%eax
	add %rax,%rcx
	movzbl (%rdi,%rcx),%eax
	movzbl (%rsi,%rcx),%edx
	sub %edx,%eax
	vzeroupper
	ret
	.size memcmp,.-memcmp
	.section .note.GNU-stack,"",@progbits
