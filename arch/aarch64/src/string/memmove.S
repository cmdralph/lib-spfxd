/*
 * lib-spfxd — memmove (AArch64, NEON).
 *
 * The forward copy of memcpy is correct whenever dst - src >= n
 * (unsigned: dst below src, or no overlap) and for every n <= 128 (those
 * paths load everything before storing).  The remaining case, a large
 * copy to a higher overlapping address, runs backward: save the last 16
 * and the first 64 source bytes, copy 64 bytes per iteration downward to
 * 16-byte-aligned destination ends, then store the saved bytes.
 */
	.text
	.global memmove
	.type memmove, %function
	.p2align 6
memmove:
	sub x3, x0, x1
	cmp x3, x2
	b.hs __spfxd_memcpy
	cmp x2, #128
	b.ls __spfxd_memcpy
	add x9, x1, x2                  /* src end */
	add x10, x0, x2                 /* dst end */
	ldr q16, [x9, #-16]
	ldp q4, q5, [x1]
	ldp q6, q7, [x1, #32]
	and x11, x10, #15
	sub x12, x10, x11               /* aligned dst end */
	sub x14, x9, x11
	add x15, x0, #64
1:	ldp q0, q1, [x14, #-32]
	ldp q2, q3, [x14, #-64]
	sub x14, x14, #64
	stp q0, q1, [x12, #-32]
	stp q2, q3, [x12, #-64]
	sub x12, x12, #64
	cmp x12, x15
	b.hi 1b
	str q16, [x10, #-16]
	stp q4, q5, [x0]
	stp q6, q7, [x0, #32]
	ret
	.size memmove, . - memmove

	.section .note.GNU-stack, "", %progbits
