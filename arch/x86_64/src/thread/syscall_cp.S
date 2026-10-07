/*
 * lib-spfxd — long __syscall_cp_asm(volatile int *cancel, long nr,
 *                                   long a, long b, long c, long d, long e, long f)
 *
 * A system call that is also a thread-cancellation point.  The region
 * [__cp_begin, __cp_end) covers the check of the cancel flag up to and
 * including the `syscall` instruction.  If the cancellation signal arrives
 * while the thread's pc is inside that region, the handler rewrites the
 * saved pc to __cp_cancel, so the syscall either never starts or (because
 * the handler is installed with SA_RESTART, which rewinds pc onto the
 * syscall instruction) is abandoned before completing.  A syscall that has
 * already completed (pc == __cp_end) keeps its result.
 *
 * C arguments:  rdi=cancel rsi=nr rdx=a rcx=b r8=c r9=d 8(%rsp)=e 16(%rsp)=f
 * Kernel:       rax=nr rdi=a rsi=b rdx=c r10=d r8=e r9=f
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
	.type __syscall_cp_asm,@function
__syscall_cp_asm:
__cp_begin:
	mov (%rdi),%eax
	test %eax,%eax
	jnz __cp_cancel
	mov %rsi,%rax
	mov %rdx,%rdi
	mov %rcx,%rsi
	mov %r8,%rdx
	mov %r9,%r10
	mov 8(%rsp),%r8
	mov 16(%rsp),%r9
	syscall
__cp_end:
	ret
__cp_cancel:
	jmp __cancel
	.size __syscall_cp_asm,.-__syscall_cp_asm

	.section .note.GNU-stack,"",@progbits
