/*
 * lib-spfxd — strlen (x86-64, SSE2 baseline and AVX2).
 *
 * Only aligned 16-byte loads are used, so no load crosses a page boundary
 * and nothing past the terminator's page is touched.  The first block is
 * masked to ignore bytes before the start; after reaching 64-byte
 * alignment the loop examines 64 bytes per iteration by folding four
 * vectors with pminub (a zero byte survives the minimum).
 *
 * The AVX2 variant (chosen at run time from __cpu_features) does the same
 * with 32-byte vectors and 128 bytes per iteration.
 */
#include "cpu.h"
	.text
	.global strlen
	.type strlen,@function
	.p2align 5
strlen:
	testb $CPU_AVX2,__cpu_features(%rip)
	jnz .Lavx2
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

	.p2align 5
.Lavx2:
	vpxor %xmm0,%xmm0,%xmm0
	mov %edi,%ecx
	and $31,%ecx
	mov %rdi,%rax
	and $-32,%rax
	vpcmpeqb (%rax),%ymm0,%ymm1
	vpmovmskb %ymm1,%edx
	shr %cl,%edx
	test %edx,%edx
	jnz .La_head
	/* 32-byte steps up to 128-byte alignment */
1:	add $32,%rax
	test $127,%al
	jz .La_loop
	vpcmpeqb (%rax),%ymm0,%ymm1
	vpmovmskb %ymm1,%edx
	test %edx,%edx
	jz 1b
	jmp .La_found

	.p2align 5
.La_loop:
	vmovdqa (%rax),%ymm1
	vpminub 32(%rax),%ymm1,%ymm2
	vmovdqa 64(%rax),%ymm3
	vpminub 96(%rax),%ymm3,%ymm4
	vpminub %ymm2,%ymm4,%ymm4
	vpcmpeqb %ymm0,%ymm4,%ymm4
	vpmovmskb %ymm4,%edx
	sub $-128,%rax
	test %edx,%edx
	jz .La_loop
	add $-128,%rax
.La_tail:
	vpcmpeqb (%rax),%ymm0,%ymm1
	vpmovmskb %ymm1,%ecx
	vpcmpeqb 32(%rax),%ymm0,%ymm1
	vpmovmskb %ymm1,%edx
	shl $32,%rdx
	or %rcx,%rdx
	jnz .La_found
	vpcmpeqb 64(%rax),%ymm0,%ymm1
	vpmovmskb %ymm1,%ecx
	vpcmpeqb 96(%rax),%ymm0,%ymm1
	vpmovmskb %ymm1,%edx
	shl $32,%rdx
	or %rcx,%rdx
	add $64,%rax
.La_found:
	bsf %rdx,%rdx
	add %rdx,%rax
	sub %rdi,%rax
	vzeroupper
	ret
.La_head:
	bsf %edx,%eax
	vzeroupper
	ret
	.size strlen,.-strlen
	.section .note.GNU-stack,"",@progbits
