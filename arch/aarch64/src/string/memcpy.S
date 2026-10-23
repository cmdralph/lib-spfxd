/*
 * lib-spfxd — memcpy (AArch64, NEON); also the forward path of memmove.
 *
 *   n <= 16       two overlapping scalar loads/stores (8, 4, or bytes)
 *   n <= 32/64/128  overlapping 16-byte head and tail vectors
 *   n > 128       load the first 16 and the last 64 bytes, copy 64 bytes
 *                 per iteration to 16-byte-aligned destinations, then
 *                 store the saved head and tail
 * Every path loads before it stores whatever it may overwrite: the small
 * cases load everything first, and the loop only writes below the source
 * it has yet to read when dst < src.  memmove therefore uses this code
 * except for large copies to a higher, overlapping destination.
 */
	.text
	.global memcpy
	.type memcpy, %function
	.global __spfxd_memcpy
	.hidden __spfxd_memcpy
	.p2align 6
memcpy:
__spfxd_memcpy:
	add x9, x1, x2                  /* src end */
	add x10, x0, x2                 /* dst end */
	cmp x2, #16
	b.hi 2f
	cmp x2, #8
	b.lo 1f
	ldr x6, [x1]
	ldr x7, [x9, #-8]
	str x6, [x0]
	str x7, [x10, #-8]
	ret
1:	cmp x2, #4
	b.lo 0f
	ldr w6, [x1]
	ldr w7, [x9, #-4]
	str w6, [x0]
	str w7, [x10, #-4]
	ret
0:	cbz x2, 9f
	lsr x5, x2, #1                  /* 1..3 bytes: first, middle, last */
	ldrb w6, [x1]
	ldrb w8, [x1, x5]
	ldrb w7, [x9, #-1]
	strb w6, [x0]
	strb w8, [x0, x5]
	strb w7, [x10, #-1]
9:	ret

2:	cmp x2, #32
	b.hi 3f
	ldr q0, [x1]
	ldr q1, [x9, #-16]
	str q0, [x0]
	str q1, [x10, #-16]
	ret
3:	cmp x2, #64
	b.hi 4f
	ldp q0, q1, [x1]
	ldp q2, q3, [x9, #-32]
	stp q0, q1, [x0]
	stp q2, q3, [x10, #-32]
	ret
4:	cmp x2, #128
	b.hi 5f
	ldp q0, q1, [x1]
	ldp q2, q3, [x1, #32]
	ldp q4, q5, [x9, #-64]
	ldp q6, q7, [x9, #-32]
	stp q0, q1, [x0]
	stp q2, q3, [x0, #32]
	stp q4, q5, [x10, #-64]
	stp q6, q7, [x10, #-32]
	ret

5:	ldr q16, [x1]
	ldp q4, q5, [x9, #-64]
	ldp q6, q7, [x9, #-32]
	add x12, x0, #16
	bic x12, x12, #15               /* first aligned dst above d */
	sub x13, x12, x0
	add x14, x1, x13
	sub x15, x10, #64
6:	ldp q0, q1, [x14]
	ldp q2, q3, [x14, #32]
	add x14, x14, #64
	stp q0, q1, [x12]
	stp q2, q3, [x12, #32]
	add x12, x12, #64
	cmp x12, x15
	b.lo 6b
	str q16, [x0]
	stp q4, q5, [x10, #-64]
	stp q6, q7, [x10, #-32]
	ret
	.size memcpy, . - memcpy

	.section .note.GNU-stack, "", %progbits
