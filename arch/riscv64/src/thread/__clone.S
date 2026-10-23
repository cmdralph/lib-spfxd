/*
 * lib-spfxd — int __clone(int (*fn)(void *), void *stack, int flags,
 *                         void *arg, pid_t *ptid, void *tls, pid_t *ctid)
 *
 * C arguments:  a0=fn a1=stack a2=flags a3=arg a4=ptid a5=tls a6=ctid
 * Kernel clone: a7=220 a0=flags a1=newsp a2=ptid a3=tls a4=ctid
 *
 * fn and arg are stored on the child's stack (16-byte aligned) because
 * the child's stack is the only memory known to be its own.  The child
 * calls fn and ends the *thread* (SYS_exit) with its result; it never
 * returns from here.
 */
	.text
	.global __clone
	.hidden __clone
	.type __clone,%function
__clone:
	andi a1, a1, -16
	addi a1, a1, -16
	sd a0, 0(a1)
	sd a3, 8(a1)
	slli a0, a2, 32                 /* flags: zero-extend the int */
	srli a0, a0, 32
	mv a2, a4
	mv a3, a5
	mv a4, a6
	li a7, 220
	ecall
	beqz a0, 1f
	ret
1:	/* child */
	li ra, 0
	li s0, 0
	ld a1, 0(sp)
	ld a0, 8(sp)
	jalr a1
	li a7, 93
	ecall
	unimp
	.size __clone,.-__clone

	.section .note.GNU-stack,"",%progbits
