/*
 * lib-spfxd — memrchr (AArch64, NEON).
 *
 * Scans aligned 16-byte blocks backward from the one holding the last
 * byte.  In the 4-bits-per-byte match mask (shrn #4) the highest set
 * nibble is the last match: byte = 15 - clz / 4.  The first block is
 * shifted left to drop the bytes at or past the end; a match below s is
 * no match.  Aligned blocks never leave the pages of [s, s + n).
 */
	.text
	.global memrchr
	.type memrchr, %function
	.p2align 6
memrchr:
	cbz x2, 9f
	mov x10, x0                     /* s */
	add x8, x0, x2                  /* end */
	dup v7.16b, w1
	sub x5, x8, #1
	bic x3, x5, #15                 /* block holding the last byte */
	and x5, x5, #15                 /* its index k in that block */
	ld1 {v0.16b}, [x3]
	cmeq v0.16b, v0.16b, v7.16b
	shrn v0.8b, v0.8h, #4
	fmov x4, d0
	eor x6, x5, #15
	lsl x6, x6, #2
	lsl x4, x4, x6                  /* drop bytes past the end */
	cbz x4, 1f
	clz x4, x4
	add x0, x3, x5
	sub x0, x0, x4, lsr #2          /* k - clz / 4 */
	cmp x0, x10
	csel x0, x0, xzr, hs
	ret

1:	cmp x3, x10
	b.ls 9f                         /* nothing at or above s is left */
	sub x3, x3, #16
	ld1 {v0.16b}, [x3]
	cmeq v0.16b, v0.16b, v7.16b
	umaxp v1.16b, v0.16b, v0.16b
	fmov x4, d1
	cbz x4, 1b
	shrn v0.8b, v0.8h, #4
	fmov x4, d0
	clz x4, x4
	add x0, x3, #15
	sub x0, x0, x4, lsr #2
	cmp x0, x10
	csel x0, x0, xzr, hs
	ret

9:	mov x0, #0
	ret
	.size memrchr, . - memrchr

	.section .note.GNU-stack, "", %progbits
