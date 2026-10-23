/*
 * lib-spfxd — shared body of strcmp and strncmp (AArch64, NEON),
 * included with STRCMP_NAME defined and STRCMP_BOUNDED set to 1 for
 * strncmp (count in x2) or 0 for strcmp.
 *
 * The two strings have unrelated alignments, so blocks are loaded
 * unaligned, but only while neither pointer is within 15 bytes of a page
 * end; otherwise single bytes are compared until both are past it.  In a
 * block the scan stops at the first byte that differs or ends a: the
 * nibble mask of (a == 0) | ~(a == b).  strncmp takes bytes once fewer
 * than 16 remain.
 */
	.text
	.global STRCMP_NAME
	.type STRCMP_NAME, %function
	.p2align 6
STRCMP_NAME:
#if STRCMP_BOUNDED
	cbz x2, 9f
#endif
1:
#if STRCMP_BOUNDED
	cmp x2, #16
	b.lo 5f
#endif
	and x3, x0, #4095
	and x4, x1, #4095
	cmp x3, #4080
	b.hi 5f
	cmp x4, #4080
	b.hi 5f
	ldr q0, [x0]
	ldr q1, [x1]
	cmeq v2.16b, v0.16b, v1.16b
	cmeq v3.16b, v0.16b, #0
	orn v2.16b, v3.16b, v2.16b      /* end of a, or a difference */
	umaxp v4.16b, v2.16b, v2.16b
	fmov x5, d4
	cbnz x5, 3f
	add x0, x0, #16
	add x1, x1, #16
#if STRCMP_BOUNDED
	subs x2, x2, #16
	b.eq 9f                         /* count used up exactly */
#endif
	b 1b

3:	shrn v2.8b, v2.8h, #4
	fmov x5, d2
	rbit x5, x5
	clz x5, x5
	lsr x5, x5, #2
	ldrb w6, [x0, x5]
	ldrb w7, [x1, x5]
	sub w0, w6, w7
	ret

5:	ldrb w6, [x0], #1
	ldrb w7, [x1], #1
	cmp w6, w7
	b.ne 6f
	cbz w6, 9f
#if STRCMP_BOUNDED
	subs x2, x2, #1
	b.ne 1b
	b 9f
#else
	b 1b
#endif
6:	sub w0, w6, w7
	ret
9:	mov w0, #0
	ret
	.size STRCMP_NAME, . - STRCMP_NAME

	.section .note.GNU-stack, "", %progbits
