/*
 * lib-spfxd — strcmp (x86-64, SSE2 baseline and AVX2).
 *
 * Compares 16 bytes per step with unaligned loads at the same offset in
 * both strings.  A step is only taken when neither 16-byte window crosses
 * a page boundary; otherwise a single byte is compared, which advances
 * the offset past the boundary within at most 15 steps.  The first
 * position that differs or holds the terminator decides the result.
 *
 * AVX2: the number of whole 32-byte steps before either string reaches a
 * page boundary is computed once, and that many steps run without any
 * further page checks (two per iteration when possible).  At a boundary
 * bytes are compared singly until the nearer pointer has crossed it.
 * For each step, min(cmpeq(a, b), a) is zero exactly where the bytes
 * differ or a holds the terminator.
 */
#include "cpu.h"
	.text
	.global strcmp
	.type strcmp,@function
	.p2align 5
strcmp:
	testb $CPU_AVX2,__cpu_features(%rip)
	jnz .Lavx2
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

	.p2align 5
.Lavx2:
	vpxor %xmm0,%xmm0,%xmm0
.La_page:
	mov %edi,%eax
	and $4095,%eax
	mov %esi,%ecx
	and $4095,%ecx
	cmp %ecx,%eax
	cmovb %ecx,%eax                    /* larger page offset */
	mov $4096,%edx
	sub %eax,%edx                      /* bytes before the nearer boundary */
	mov %edx,%r8d
	shr $5,%edx                        /* whole 32-byte steps */
	jz .La_bytes
	test $1,%dl
	jz .La_even
	/* odd count: one single step first */
	vmovdqu (%rdi),%ymm1
	vpcmpeqb (%rsi),%ymm1,%ymm2
	vpminub %ymm1,%ymm2,%ymm2
	vpcmpeqb %ymm0,%ymm2,%ymm2
	vpmovmskb %ymm2,%eax
	test %eax,%eax
	jnz .La_diff
	add $32,%rdi
	add $32,%rsi
	dec %edx
	jz .La_page
.La_even:
	cmp $4,%edx
	jb .La_two
	.p2align 4
.La_four:
	vmovdqu (%rdi),%ymm1
	vmovdqu 32(%rdi),%ymm3
	vmovdqu 64(%rdi),%ymm6
	vmovdqu 96(%rdi),%ymm8
	vpcmpeqb (%rsi),%ymm1,%ymm2
	vpcmpeqb 32(%rsi),%ymm3,%ymm4
	vpcmpeqb 64(%rsi),%ymm6,%ymm7
	vpcmpeqb 96(%rsi),%ymm8,%ymm9
	vpminub %ymm1,%ymm2,%ymm2
	vpminub %ymm3,%ymm4,%ymm4
	vpminub %ymm6,%ymm7,%ymm7
	vpminub %ymm8,%ymm9,%ymm9
	vpminub %ymm2,%ymm4,%ymm5
	vpminub %ymm7,%ymm9,%ymm10
	vpminub %ymm5,%ymm10,%ymm10
	vpcmpeqb %ymm0,%ymm10,%ymm10
	vpmovmskb %ymm10,%eax
	test %eax,%eax
	jnz .La_diff4
	sub $-128,%rdi
	sub $-128,%rsi
	sub $4,%edx
	cmp $4,%edx
	jae .La_four
	test %edx,%edx
	jz .La_page
	.p2align 4
.La_two:
	vmovdqu (%rdi),%ymm1
	vmovdqu 32(%rdi),%ymm3
	vpcmpeqb (%rsi),%ymm1,%ymm2
	vpcmpeqb 32(%rsi),%ymm3,%ymm4
	vpminub %ymm1,%ymm2,%ymm2
	vpminub %ymm3,%ymm4,%ymm4
	vpminub %ymm2,%ymm4,%ymm5
	vpcmpeqb %ymm0,%ymm5,%ymm5
	vpmovmskb %ymm5,%eax
	test %eax,%eax
	jnz .La_diff2
	add $64,%rdi
	add $64,%rsi
	sub $2,%edx
	jnz .La_two
	jmp .La_page
.La_diff4:
	vpcmpeqb %ymm0,%ymm5,%ymm5
	vpmovmskb %ymm5,%eax
	test %eax,%eax
	jnz .La_diff2
	vmovdqa %ymm7,%ymm2
	vmovdqa %ymm9,%ymm4
	add $64,%rdi
	add $64,%rsi
.La_diff2:
	vpcmpeqb %ymm0,%ymm2,%ymm2
	vpmovmskb %ymm2,%eax
	test %eax,%eax
	jnz .La_diff
	vpcmpeqb %ymm0,%ymm4,%ymm4
	vpmovmskb %ymm4,%eax
	add $32,%rdi
	add $32,%rsi
.La_diff:
	bsf %eax,%eax
	movzbl (%rdi,%rax),%ecx
	movzbl (%rsi,%rax),%edx
	mov %ecx,%eax
	sub %edx,%eax
	vzeroupper
	ret
	/* fewer than 32 bytes to the nearer boundary: step bytewise past it */
.La_bytes:
	movzbl (%rdi),%eax
	movzbl (%rsi),%ecx
	sub %ecx,%eax
	jnz .La_ret
	test %ecx,%ecx
	jz .La_ret
	inc %rdi
	inc %rsi
	dec %r8d
	jnz .La_bytes
	jmp .La_page
.La_ret:
	vzeroupper
	ret
	.size strcmp,.-strcmp
	.section .note.GNU-stack,"",@progbits
