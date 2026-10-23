/*
 * lib-spfxd — shared body of strchr and strchrnul (AArch64, NEON),
 * included with STRCHR_NAME defined and STRCHR_NULL set to 1 for strchr
 * (NULL when the terminator comes first) or 0 for strchrnul.
 *
 * Each aligned 16-byte block is compared against c and against 0; the
 * first byte that is either stops the scan (shrn #4 mask, rbit + clz).
 * The first block is masked below s.  Aligned loads never cross a page.
 */
	.text
	.global STRCHR_NAME
	.type STRCHR_NAME, %function
	.p2align 6
STRCHR_NAME:
	dup v7.16b, w1
	bic x3, x0, #15
	ld1 {v0.16b}, [x3]
	cmeq v1.16b, v0.16b, v7.16b
	cmeq v0.16b, v0.16b, #0
	orr v0.16b, v0.16b, v1.16b
	shrn v0.8b, v0.8h, #4
	fmov x4, d0
	lsl x5, x0, #2
	lsr x4, x4, x5
	cbz x4, 1f
	rbit x4, x4
	clz x4, x4
	add x0, x0, x4, lsr #2
	b 8f

1:	add x3, x3, #16
2:	ld1 {v0.16b}, [x3]
	cmeq v1.16b, v0.16b, v7.16b
	cmeq v0.16b, v0.16b, #0
	orr v0.16b, v0.16b, v1.16b
	umaxp v2.16b, v0.16b, v0.16b
	fmov x4, d2
	cbnz x4, 3f
	add x3, x3, #16
	b 2b

3:	shrn v0.8b, v0.8h, #4
	fmov x4, d0
	rbit x4, x4
	clz x4, x4
	add x0, x3, x4, lsr #2

8:
#if STRCHR_NULL
	ldrb w4, [x0]
	cmp w4, w1, uxtb
	csel x0, x0, xzr, eq
#endif
	ret
	.size STRCHR_NAME, . - STRCHR_NAME

	.section .note.GNU-stack, "", %progbits
