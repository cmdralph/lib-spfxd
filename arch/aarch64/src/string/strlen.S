/*
 * lib-spfxd — strlen (AArch64, NEON).
 *
 * Only 16-byte-aligned loads (and 32-byte-aligned pairs) are used, so no
 * load crosses a page boundary and nothing past the terminator's page is
 * read.  A compare result is narrowed to a 64-bit mask with 4 bits per
 * byte (shrn #4); the first match is then rbit + clz, divided by 4.  The
 * first block is shifted right by 4 * (s & 15) bits to ignore the bytes
 * before s.  The main loop takes 32 bytes per iteration: uminp folds the
 * two vectors (a zero byte survives the minimum), and umaxp folds the
 * compare result to 64 bits for one branch.
 */
	.text
	.global strlen
	.type strlen, %function
	.p2align 6
strlen:
	bic x1, x0, #15
	ld1 {v0.16b}, [x1]
	cmeq v0.16b, v0.16b, #0
	shrn v0.8b, v0.8h, #4
	fmov x2, d0
	lsl x3, x0, #2                  /* lsr uses the count mod 64 */
	lsr x2, x2, x3
	cbz x2, 1f
	rbit x2, x2
	clz x2, x2
	lsr x0, x2, #2
	ret

1:	add x1, x1, #16
	tbz x1, #4, 2f                  /* not 32-aligned: one more block */
	ld1 {v0.16b}, [x1]
	cmeq v0.16b, v0.16b, #0
	umaxp v1.16b, v0.16b, v0.16b
	fmov x2, d1
	cbnz x2, 4f
	add x1, x1, #16

2:	ldp q0, q1, [x1]
	uminp v2.16b, v0.16b, v1.16b
	cmeq v2.16b, v2.16b, #0
	umaxp v2.16b, v2.16b, v2.16b
	fmov x2, d2
	cbnz x2, 3f
	add x1, x1, #32
	b 2b

3:	cmeq v0.16b, v0.16b, #0
	umaxp v2.16b, v0.16b, v0.16b
	fmov x2, d2
	cbnz x2, 4f
	add x1, x1, #16
	mov v0.16b, v1.16b
	cmeq v0.16b, v0.16b, #0

4:	/* v0 holds the compare result for the block at x1 */
	shrn v0.8b, v0.8h, #4
	fmov x2, d0
	rbit x2, x2
	clz x2, x2
	add x1, x1, x2, lsr #2
	sub x0, x1, x0
	ret
	.size strlen, . - strlen

	.section .note.GNU-stack, "", %progbits
