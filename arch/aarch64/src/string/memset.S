/*
 * lib-spfxd — memset (AArch64, NEON).
 *
 * Overlapping head and tail stores for n <= 64 (scalar below 16 bytes);
 * above that a 16-byte head, 64 bytes per iteration to 16-byte-aligned
 * addresses while a whole block fits before the last 64 bytes, then a
 * 64-byte tail.  Nothing outside [d, d + n) is written.
 */
	.text
	.global memset
	.type memset, %function
	.p2align 6
memset:
	dup v0.16b, w1
	add x10, x0, x2                 /* end */
	cmp x2, #16
	b.hi 2f
	fmov x6, d0
	cmp x2, #8
	b.lo 1f
	str x6, [x0]
	str x6, [x10, #-8]
	ret
1:	cmp x2, #4
	b.lo 0f
	str w6, [x0]
	str w6, [x10, #-4]
	ret
0:	cbz x2, 9f
	lsr x5, x2, #1                  /* 1..3 bytes: first, middle, last */
	strb w1, [x0]
	strb w1, [x0, x5]
	strb w1, [x10, #-1]
9:	ret

2:	cmp x2, #32
	b.hi 3f
	str q0, [x0]
	str q0, [x10, #-16]
	ret
3:	cmp x2, #64
	b.hi 4f
	stp q0, q0, [x0]
	stp q0, q0, [x10, #-32]
	ret
4:	str q0, [x0]
	add x12, x0, #16
	bic x12, x12, #15
	sub x15, x10, #64
	cmp x12, x15
	b.hs 6f
5:	stp q0, q0, [x12]
	stp q0, q0, [x12, #32]
	add x12, x12, #64
	cmp x12, x15
	b.lo 5b
6:	stp q0, q0, [x10, #-64]
	stp q0, q0, [x10, #-32]
	ret
	.size memset, . - memset

	.section .note.GNU-stack, "", %progbits
