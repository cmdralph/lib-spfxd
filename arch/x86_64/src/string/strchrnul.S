/*
 * lib-spfxd — strchrnul (x86-64, SSE2 baseline and AVX2).
 *
 * Each aligned 16-byte block is compared against both the target byte and
 * zero; the first position matching either ends the search.
 *
 * AVX2: min(v ^ c, v) has a zero byte exactly where v holds c or the
 * terminator, so four 32-byte vectors fold into one test per 128 bytes.
 * Loads are aligned and the 128-byte groups are 128-byte aligned, so no
 * load touches a page the string does not reach.
 */
#include "cpu.h"
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
	testb $CPU_AVX2,__cpu_features(%rip)
	jnz .Lavx2
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

	.p2align 5
.Lavx2:
	vmovd %esi,%xmm1
	vpbroadcastb %xmm1,%ymm1
	vpxor %xmm0,%xmm0,%xmm0
	mov %edi,%ecx
	and $31,%ecx
	mov %rdi,%rax
	and $-32,%rax
	vmovdqa (%rax),%ymm2
	vpxor %ymm1,%ymm2,%ymm3
	vpminub %ymm2,%ymm3,%ymm3
	vpcmpeqb %ymm0,%ymm3,%ymm3
	vpmovmskb %ymm3,%edx
	shr %cl,%edx
	shl %cl,%edx
	test %edx,%edx
	jnz .La_found
1:	add $32,%rax
	test $127,%al
	jz .La_loop
	vmovdqa (%rax),%ymm2
	vpxor %ymm1,%ymm2,%ymm3
	vpminub %ymm2,%ymm3,%ymm3
	vpcmpeqb %ymm0,%ymm3,%ymm3
	vpmovmskb %ymm3,%edx
	test %edx,%edx
	jz 1b
	jmp .La_found

	.p2align 5
.La_loop:
	vmovdqa (%rax),%ymm2
	vpxor %ymm1,%ymm2,%ymm3
	vpminub %ymm2,%ymm3,%ymm3
	vmovdqa 32(%rax),%ymm4
	vpxor %ymm1,%ymm4,%ymm5
	vpminub %ymm4,%ymm5,%ymm5
	vmovdqa 64(%rax),%ymm6
	vpxor %ymm1,%ymm6,%ymm7
	vpminub %ymm6,%ymm7,%ymm7
	vmovdqa 96(%rax),%ymm8
	vpxor %ymm1,%ymm8,%ymm9
	vpminub %ymm8,%ymm9,%ymm9
	vpminub %ymm3,%ymm5,%ymm10
	vpminub %ymm7,%ymm9,%ymm11
	vpminub %ymm10,%ymm11,%ymm11
	vpcmpeqb %ymm0,%ymm11,%ymm11
	vpmovmskb %ymm11,%edx
	sub $-128,%rax
	test %edx,%edx
	jz .La_loop
	add $-128,%rax
.La_tail:
	vpcmpeqb %ymm0,%ymm3,%ymm3
	vpmovmskb %ymm3,%ecx
	vpcmpeqb %ymm0,%ymm5,%ymm5
	vpmovmskb %ymm5,%edx
	shl $32,%rdx
	or %rcx,%rdx
	jnz .La_found
	vpcmpeqb %ymm0,%ymm7,%ymm7
	vpmovmskb %ymm7,%ecx
	vpcmpeqb %ymm0,%ymm9,%ymm9
	vpmovmskb %ymm9,%edx
	shl $32,%rdx
	or %rcx,%rdx
	add $64,%rax
.La_found:
	bsf %rdx,%rdx
	add %rdx,%rax
	vzeroupper
	ret
	.size strchrnul,.-strchrnul
	.size __strchrnul,.-__strchrnul

	.section .note.GNU-stack,"",@progbits
