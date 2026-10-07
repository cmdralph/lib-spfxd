/*
 * lib-spfxd — int __clone(int (*fn)(void *), void *stack, int flags,
 *                         void *arg, pid_t *ptid, void *tls, pid_t *ctid)
 *
 * C arguments:  x0=fn x1=stack w2=flags x3=arg x4=ptid x5=tls x6=ctid
 * Kernel clone: x8=220 x0=flags x1=newsp x2=ptid x3=tls x4=ctid
 *
 * fn and arg are pushed on the child's stack (16-byte aligned) because the
 * child's stack is the only memory known to be its own.  The child clears
 * the frame pointer, calls fn and ends the *thread* (SYS_exit) with its
 * result; it never returns from here.
 */
	.text
	.global __clone
	.hidden __clone
	.type __clone,%function
__clone:
	and x1, x1, #-16
	stp x0, x3, [x1, #-16]!
	uxtw x0, w2
	mov x2, x4
	mov x3, x5
	mov x4, x6
	mov x8, #220
	svc #0
	cbz x0, 1f
	ret
1:	/* child */
	.cfi_startproc
	.cfi_undefined x30
	mov x29, #0
	ldp x1, x0, [sp], #16
	blr x1
	mov x8, #93
	svc #0
	udf #0
	.cfi_endproc
	.size __clone,.-__clone

	.section .note.GNU-stack,"",%progbits
