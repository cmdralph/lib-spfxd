/*
 * lib-spfxd — memcmp (AArch64, NEON).
 *
 * n >= 16: unaligned 16-byte blocks from both operands, then one last
 * block ending exactly at a + n (overlapping the previous one), so
 * nothing outside [a, a + n) and [b, b + n) is read.  A mismatch is the
 * first set nibble of ~(a == b) narrowed with shrn #4.
 * n < 16: overlapping 8- or 4-byte words compared after byte reversal
 * (big-endian order equals byte order), or single bytes.
 */
	.text
	.global memcmp
	.type memcmp, %function
	.p2align 6
memcmp:
	cmp x2, #16
	b.lo 8f
	add x7, x0, x2
	sub x7, x7, #16                 /* last block of a */
	add x6, x1, x2
	sub x6, x6, #16                 /* last block of b */
1:	ldr q0, [x0]
	ldr q1, [x1]
	cmeq v2.16b, v0.16b, v1.16b
	not v2.16b, v2.16b
	umaxp v3.16b, v2.16b, v2.16b
	fmov x4, d3
	cbnz x4, 7f
	add x0, x0, #16
	add x1, x1, #16
	cmp x0, x7
	b.lo 1b
	mov x0, x7
	mov x1, x6
	ldr q0, [x0]
	ldr q1, [x1]
	cmeq v2.16b, v0.16b, v1.16b
	not v2.16b, v2.16b
	umaxp v3.16b, v2.16b, v2.16b
	fmov x4, d3
	cbnz x4, 7f
	mov w0, #0
	ret
7:	shrn v2.8b, v2.8h, #4
	fmov x4, d2
	rbit x4, x4
	clz x4, x4
	lsr x4, x4, #2
	ldrb w5, [x0, x4]
	ldrb w6, [x1, x4]
	sub w0, w5, w6
	ret

8:	cmp x2, #8
	b.lo 9f
	ldr x3, [x0]
	ldr x4, [x1]
	cmp x3, x4
	b.ne 10f
	sub x2, x2, #8
	ldr x3, [x0, x2]
	ldr x4, [x1, x2]
	cmp x3, x4
	b.ne 10f
	mov w0, #0
	ret
10:	rev x3, x3
	rev x4, x4
	cmp x3, x4
	mov w0, #1
	cneg w0, w0, lo
	ret

9:	cmp x2, #4
	b.lo 11f
	ldr w3, [x0]
	ldr w4, [x1]
	cmp w3, w4
	b.ne 12f
	sub x2, x2, #4
	ldr w3, [x0, x2]
	ldr w4, [x1, x2]
	cmp w3, w4
	b.ne 12f
	mov w0, #0
	ret
12:	rev w3, w3
	rev w4, w4
	cmp w3, w4
	mov w0, #1
	cneg w0, w0, lo
	ret

11:	cbz x2, 13f
	ldrb w3, [x0], #1
	ldrb w4, [x1], #1
	subs w5, w3, w4                 /* not w0: x0 is still the pointer */
	b.ne 14f
	sub x2, x2, #1
	b 11b
13:	mov w5, #0
14:	mov w0, w5
	ret
	.size memcmp, . - memcmp

	.section .note.GNU-stack, "", %progbits
