/*
 * lib-spfxd — memchr (AArch64, NEON).
 *
 * Same scheme as strlen: aligned 16-byte blocks (32 bytes per loop
 * iteration once 32-aligned), the first block masked below s, matches
 * located with shrn #4 / rbit / clz.  The end s + n saturates at the top
 * of the address space (callers such as rawmemchr pass SIZE_MAX), and a
 * match at or beyond the end is no match.  Aligned blocks may extend past
 * s + n but never into another page.
 */
	.text
	.global memchr
	.type memchr, %function
	.p2align 6
memchr:
	cbz x2, 9f
	adds x8, x0, x2
	csinv x8, x8, xzr, cc           /* end, saturated */
	dup v7.16b, w1
	bic x3, x0, #15
	ld1 {v0.16b}, [x3]
	cmeq v0.16b, v0.16b, v7.16b
	shrn v0.8b, v0.8h, #4
	fmov x4, d0
	lsl x5, x0, #2
	lsr x4, x4, x5
	cbz x4, 1f
	rbit x4, x4
	clz x4, x4
	add x0, x0, x4, lsr #2
	cmp x0, x8
	csel x0, x0, xzr, lo
	ret

1:	add x3, x3, #16
	cmp x3, x8
	b.hs 9f
	tbz x3, #4, 2f
	ld1 {v0.16b}, [x3]
	cmeq v0.16b, v0.16b, v7.16b
	umaxp v1.16b, v0.16b, v0.16b
	fmov x4, d1
	cbnz x4, 5f
	add x3, x3, #16

2:	cmp x3, x8
	b.hs 9f
	ldp q0, q1, [x3]
	cmeq v0.16b, v0.16b, v7.16b
	cmeq v1.16b, v1.16b, v7.16b
	orr v2.16b, v0.16b, v1.16b
	umaxp v2.16b, v2.16b, v2.16b
	fmov x4, d2
	cbnz x4, 3f
	add x3, x3, #32
	b 2b

3:	umaxp v2.16b, v0.16b, v0.16b
	fmov x4, d2
	cbnz x4, 5f
	add x3, x3, #16
	mov v0.16b, v1.16b

5:	shrn v0.8b, v0.8h, #4
	fmov x4, d0
	rbit x4, x4
	clz x4, x4
	add x0, x3, x4, lsr #2
	cmp x0, x8
	csel x0, x0, xzr, lo
	ret

9:	mov x0, #0
	ret
	.size memchr, . - memchr

	.section .note.GNU-stack, "", %progbits
