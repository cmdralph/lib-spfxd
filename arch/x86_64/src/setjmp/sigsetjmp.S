/*
 * lib-spfxd — int sigsetjmp(sigjmp_buf env, int savemask)
 *
 * jmp_buf layout: 64 bytes of registers, then the flag word at 64 and the
 * saved signal mask at 72.  The mask is read with rt_sigprocmask directly
 * (no function call), so the stack is exactly as the caller left it when
 * we tail-jump into setjmp, which records the caller's rsp and return
 * address.  rdi is preserved in a scratch register across the syscall
 * (rcx and r11 are the only registers the kernel clobbers).
 */
	.text
	.global sigsetjmp
	.global __sigsetjmp
	.type sigsetjmp,@function
	.type __sigsetjmp,@function
sigsetjmp:
__sigsetjmp:
	mov %esi,%eax
	mov %rax,64(%rdi)
	test %esi,%esi
	jz 1f
	mov %rdi,%r8
	lea 72(%rdi),%rdx      /* oldset */
	xor %esi,%esi          /* set = NULL */
	xor %edi,%edi          /* how (ignored when set is NULL) */
	mov $8,%r10d           /* kernel sigset size */
	mov $14,%eax           /* rt_sigprocmask */
	syscall
	mov %r8,%rdi
1:	jmp setjmp
	.size sigsetjmp,.-sigsetjmp
	.size __sigsetjmp,.-__sigsetjmp

	.section .note.GNU-stack,"",@progbits
