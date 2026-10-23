/*
 * lib-spfxd — long __syscall_cp_asm(volatile int *cancel, long nr,
 *                                   long a, long b, long c, long d, long e, long f)
 *
 * A system call that is also a thread-cancellation point.  The region
 * [__cp_begin, __cp_end) covers the check of the cancel flag up to and
 * including the `ecall`.  If the cancellation signal arrives while the pc
 * is inside it, the handler redirects the saved pc to __cp_cancel, so the
 * call either never starts or (SA_RESTART rewinds the pc onto the ecall)
 * is abandoned.  A completed call (pc == __cp_end) keeps its result.
 *
 * C arguments: a0=cancel a1=nr a2..a7=a..f;  kernel: a7=nr a0..a5=a..f
 */
	.text
	.global __cp_begin
	.hidden __cp_begin
	.global __cp_end
	.hidden __cp_end
	.global __cp_cancel
	.hidden __cp_cancel
	.hidden __cancel
	.global __syscall_cp_asm
	.hidden __syscall_cp_asm
	.type __syscall_cp_asm,%function
__syscall_cp_asm:
__cp_begin:
	lw t0, 0(a0)
	bnez t0, __cp_cancel
	mv t1, a1
	mv a0, a2
	mv a1, a3
	mv a2, a4
	mv a3, a5
	mv a4, a6
	mv a5, a7
	mv a7, t1
	ecall
__cp_end:
	ret
__cp_cancel:
	tail __cancel
	.size __syscall_cp_asm,.-__syscall_cp_asm

	.section .note.GNU-stack,"",%progbits
