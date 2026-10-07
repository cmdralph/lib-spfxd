/*
 * lib-spfxd — memcpy (x86-64, SSE2 baseline and AVX2).
 *
 *   n < 16       two possibly overlapping scalar moves of 8/4/2/1 bytes
 *   16..128      2, 4 or 8 possibly overlapping 16-byte moves, all loads
 *                issued before any store
 *   129..2047    16-byte-aligned stores in a 64-byte loop; the unaligned
 *                head and the last 64 bytes are loaded before the loop and
 *                stored with unaligned moves
 *   >= 2048      rep movsb (fast-string microcode on current CPUs)
 *
 * Every path loads all data it will later store over before storing, or
 * copies strictly forward, so the same code is a correct forward memmove
 * (dst <= src, or no overlap); memmove.S jumps to __memcpy_fwd for that.
 *
 * With AVX2, sizes 33..2047 use 32-byte moves under the same rules: up to
 * 256 bytes as overlapping head/tail vectors, beyond that a 128-byte loop
 * with 32-byte-aligned stores.
 */
#include "cpu.h"
	.text
	.global memcpy
	.global __memcpy_fwd
	.hidden __memcpy_fwd
	.type memcpy,@function
	.type __memcpy_fwd,@function
	.p2align 5
memcpy:
__memcpy_fwd:
	mov %rdi,%rax
	cmp $16,%rdx
	jb .Lsmall
	cmp $32,%rdx
	ja .Lover32
	movdqu (%rsi),%xmm0
	movdqu -16(%rsi,%rdx),%xmm1
	movdqu %xmm0,(%rdi)
	movdqu %xmm1,-16(%rdi,%rdx)
	ret

.Lsmall:
	cmp $8,%edx
	jb .Lunder8
	mov (%rsi),%rcx
	mov -8(%rsi,%rdx),%r8
	mov %rcx,(%rdi)
	mov %r8,-8(%rdi,%rdx)
	ret
.Lunder8:
	cmp $4,%edx
	jb .Lunder4
	mov (%rsi),%ecx
	mov -4(%rsi,%rdx),%r8d
	mov %ecx,(%rdi)
	mov %r8d,-4(%rdi,%rdx)
	ret
.Lunder4:
	cmp $2,%edx
	jb .Lunder2
	movzwl (%rsi),%ecx
	movzwl -2(%rsi,%rdx),%r8d
	mov %cx,(%rdi)
	mov %r8w,-2(%rdi,%rdx)
	ret
.Lunder2:
	test %edx,%edx
	jz 1f
	movzbl (%rsi),%ecx
	mov %cl,(%rdi)
1:	ret

.Lover32:
	cmp $2048,%rdx
	jae .Lrep
	testb $CPU_AVX2,__cpu_features(%rip)
	jnz .Lavx2
	cmp $64,%rdx
	ja .Lover64
	movdqu (%rsi),%xmm0
	movdqu 16(%rsi),%xmm1
	movdqu -32(%rsi,%rdx),%xmm2
	movdqu -16(%rsi,%rdx),%xmm3
	movdqu %xmm0,(%rdi)
	movdqu %xmm1,16(%rdi)
	movdqu %xmm2,-32(%rdi,%rdx)
	movdqu %xmm3,-16(%rdi,%rdx)
	ret

.Lover64:
	cmp $128,%rdx
	ja .Lbig
	movdqu (%rsi),%xmm0
	movdqu 16(%rsi),%xmm1
	movdqu 32(%rsi),%xmm2
	movdqu 48(%rsi),%xmm3
	movdqu -64(%rsi,%rdx),%xmm4
	movdqu -48(%rsi,%rdx),%xmm5
	movdqu -32(%rsi,%rdx),%xmm6
	movdqu -16(%rsi,%rdx),%xmm7
	movdqu %xmm0,(%rdi)
	movdqu %xmm1,16(%rdi)
	movdqu %xmm2,32(%rdi)
	movdqu %xmm3,48(%rdi)
	movdqu %xmm4,-64(%rdi,%rdx)
	movdqu %xmm5,-48(%rdi,%rdx)
	movdqu %xmm6,-32(%rdi,%rdx)
	movdqu %xmm7,-16(%rdi,%rdx)
	ret

.Lbig:
	cmp $2048,%rdx
	jae .Lrep
	/* head and tail first */
	movdqu (%rsi),%xmm8
	lea (%rsi,%rdx),%r9               /* src end */
	lea (%rdi,%rdx),%r10              /* dst end */
	movdqu -64(%r9),%xmm4
	movdqu -48(%r9),%xmm5
	movdqu -32(%r9),%xmm6
	movdqu -16(%r9),%xmm7
	/* align dst up to 16 */
	mov %rdi,%rcx
	neg %rcx
	and $15,%ecx
	lea (%rdi,%rcx),%r11              /* aligned dst cursor */
	add %rcx,%rsi
	sub %rcx,%rdx
	.p2align 4
1:	cmp $64,%rdx
	jbe 2f
	movdqu (%rsi),%xmm0
	movdqu 16(%rsi),%xmm1
	movdqu 32(%rsi),%xmm2
	movdqu 48(%rsi),%xmm3
	movdqa %xmm0,(%r11)
	movdqa %xmm1,16(%r11)
	movdqa %xmm2,32(%r11)
	movdqa %xmm3,48(%r11)
	add $64,%rsi
	add $64,%r11
	sub $64,%rdx
	jmp 1b
2:	movdqu %xmm4,-64(%r10)
	movdqu %xmm5,-48(%r10)
	movdqu %xmm6,-32(%r10)
	movdqu %xmm7,-16(%r10)
	movdqu %xmm8,(%rdi)
	ret

.Lrep:
	mov %rdx,%rcx
	rep movsb
	ret

	/* AVX2, 33 <= n < 2048 */
	.p2align 4
.Lavx2:
	cmp $64,%rdx
	ja 1f
	vmovdqu (%rsi),%ymm0
	vmovdqu -32(%rsi,%rdx),%ymm1
	vmovdqu %ymm0,(%rdi)
	vmovdqu %ymm1,-32(%rdi,%rdx)
	vzeroupper
	ret
1:	cmp $128,%rdx
	ja 1f
	vmovdqu (%rsi),%ymm0
	vmovdqu 32(%rsi),%ymm1
	vmovdqu -64(%rsi,%rdx),%ymm2
	vmovdqu -32(%rsi,%rdx),%ymm3
	vmovdqu %ymm0,(%rdi)
	vmovdqu %ymm1,32(%rdi)
	vmovdqu %ymm2,-64(%rdi,%rdx)
	vmovdqu %ymm3,-32(%rdi,%rdx)
	vzeroupper
	ret
1:	cmp $256,%rdx
	ja .La_loop
	vmovdqu (%rsi),%ymm0
	vmovdqu 32(%rsi),%ymm1
	vmovdqu 64(%rsi),%ymm2
	vmovdqu 96(%rsi),%ymm3
	vmovdqu -128(%rsi,%rdx),%ymm4
	vmovdqu -96(%rsi,%rdx),%ymm5
	vmovdqu -64(%rsi,%rdx),%ymm6
	vmovdqu -32(%rsi,%rdx),%ymm7
	vmovdqu %ymm0,(%rdi)
	vmovdqu %ymm1,32(%rdi)
	vmovdqu %ymm2,64(%rdi)
	vmovdqu %ymm3,96(%rdi)
	vmovdqu %ymm4,-128(%rdi,%rdx)
	vmovdqu %ymm5,-96(%rdi,%rdx)
	vmovdqu %ymm6,-64(%rdi,%rdx)
	vmovdqu %ymm7,-32(%rdi,%rdx)
	vzeroupper
	ret
.La_loop:
	vmovdqu (%rsi),%ymm8
	lea (%rsi,%rdx),%r9               /* src end */
	lea (%rdi,%rdx),%r10              /* dst end */
	vmovdqu -128(%r9),%ymm4
	vmovdqu -96(%r9),%ymm5
	vmovdqu -64(%r9),%ymm6
	vmovdqu -32(%r9),%ymm7
	mov %rdi,%rcx
	neg %rcx
	and $31,%ecx
	lea (%rdi,%rcx),%r11              /* 32-byte-aligned dst cursor */
	add %rcx,%rsi
	sub %rcx,%rdx
	.p2align 4
2:	cmp $128,%rdx
	jbe 3f
	vmovdqu (%rsi),%ymm0
	vmovdqu 32(%rsi),%ymm1
	vmovdqu 64(%rsi),%ymm2
	vmovdqu 96(%rsi),%ymm3
	vmovdqa %ymm0,(%r11)
	vmovdqa %ymm1,32(%r11)
	vmovdqa %ymm2,64(%r11)
	vmovdqa %ymm3,96(%r11)
	sub $-128,%rsi
	sub $-128,%r11
	add $-128,%rdx
	jmp 2b
3:	vmovdqu %ymm4,-128(%r10)
	vmovdqu %ymm5,-96(%r10)
	vmovdqu %ymm6,-64(%r10)
	vmovdqu %ymm7,-32(%r10)
	vmovdqu %ymm8,(%rdi)
	vzeroupper
	ret
	.size memcpy,.-memcpy
	.size __memcpy_fwd,.-__memcpy_fwd
	.section .note.GNU-stack,"",@progbits
