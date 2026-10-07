/*
 * lib-spfxd — memchr (x86-64, SSE2 baseline and AVX2).
 *
 * Aligned 16-byte blocks; matches before the start are masked off in the
 * first block and a match is accepted only if it lies within n bytes of
 * the start.  Aligned loads never cross a page, so reading the remainder
 * of the last block is safe.
 *
 * AVX2: 32-byte aligned blocks; while at least 128 bytes remain, four
 * blocks are tested at once.
 */
#include "cpu.h"
	.text
	.global memchr
	.type memchr,@function
	.p2align 5
memchr:
	test %rdx,%rdx
	jz .Lnull
	testb $CPU_AVX2,__cpu_features(%rip)
	jnz .Lavx2
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

	.p2align 5
.Lavx2:
	vmovd %esi,%xmm1
	vpbroadcastb %xmm1,%ymm1
	mov %edi,%ecx
	and $31,%ecx
	mov %rdi,%rax
	and $-32,%rax
	vpcmpeqb (%rax),%ymm1,%ymm2
	vpmovmskb %ymm2,%r8d
	shr %cl,%r8d
	test %r8d,%r8d
	jz 1f
	bsf %r8d,%r8d
	cmp %rdx,%r8
	jae .La_null
	lea (%rdi,%r8),%rax
	vzeroupper
	ret
1:	mov $32,%r9d
	sub %ecx,%r9d                      /* bytes covered by the first block */
	cmp %r9,%rdx
	jbe .La_null
	sub %r9,%rdx                       /* bytes remaining from rax + 32 */
	add $32,%rax
	cmp $128,%rdx
	jb 4f
	.p2align 4
2:	vpcmpeqb (%rax),%ymm1,%ymm2
	vpcmpeqb 32(%rax),%ymm1,%ymm3
	vpcmpeqb 64(%rax),%ymm1,%ymm4
	vpcmpeqb 96(%rax),%ymm1,%ymm5
	vpor %ymm2,%ymm3,%ymm6
	vpor %ymm4,%ymm5,%ymm7
	vpor %ymm6,%ymm7,%ymm7
	vpmovmskb %ymm7,%r8d
	test %r8d,%r8d
	jnz 3f
	sub $-128,%rax
	add $-128,%rdx
	cmp $128,%rdx
	jae 2b
	test %rdx,%rdx
	jz .La_null
	jmp 4f
3:	/* the match lies within these 128 bytes, all inside the buffer */
	vpmovmskb %ymm2,%ecx
	vpmovmskb %ymm3,%r8d
	shl $32,%r8
	or %rcx,%r8
	jnz 5f
	vpmovmskb %ymm4,%ecx
	vpmovmskb %ymm5,%r8d
	shl $32,%r8
	or %rcx,%r8
	add $64,%rax
5:	bsf %r8,%r8
	add %r8,%rax
	vzeroupper
	ret
	/* fewer than 128 bytes left: single aligned blocks */
4:	vpcmpeqb (%rax),%ymm1,%ymm2
	vpmovmskb %ymm2,%r8d
	test %r8d,%r8d
	jnz 6f
	sub $32,%rdx
	jbe .La_null
	add $32,%rax
	jmp 4b
6:	bsf %r8d,%r8d
	cmp %rdx,%r8
	jae .La_null
	add %r8,%rax
	vzeroupper
	ret
.La_null:
	xor %eax,%eax
	vzeroupper
	ret
	.size memchr,.-memchr
	.section .note.GNU-stack,"",@progbits
