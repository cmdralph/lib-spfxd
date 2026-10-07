/*
 * lib-spfxd — long __syscall_cp_asm(volatile int *cancel, long nr,
 *                                   long a, long b, long c, long d, long e, long f)
 *
 * A system call that is also a thread-cancellation point.  The region
 * [__cp_begin, __cp_end) covers the check of the cancel flag up to and
 * including the `svc` instruction.  If the cancellation signal arrives
 * while the pc is inside it, the handler redirects the saved pc to
 * __cp_cancel, so the call either never starts or (SA_RESTART rewinds the
 * pc onto the svc) is abandoned.  A completed call (pc == __cp_end) keeps
 * its result.
 *
 * C arguments: x0=cancel x1=nr x2..x7=a..f;  kernel: x8=nr x0..x5=a..f
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
	ldr w0, [x0]
	cbnz w0, __cp_cancel
	mov x8, x1
	mov x0, x2
	mov x1, x3
	mov x2, x4
	mov x3, x5
	mov x4, x6
	mov x5, x7
	svc #0
__cp_end:
	ret
__cp_cancel:
	b __cancel
	.size __syscall_cp_asm,.-__syscall_cp_asm

	.section .note.GNU-stack,"",%progbits
