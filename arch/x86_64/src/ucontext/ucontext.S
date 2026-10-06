/*
 * lib-spfxd — getcontext / setcontext / swapcontext / makecontext trampoline
 * (x86-64).
 *
 * ucontext_t offsets (see bits/signal.h):
 *   uc_mcontext.gregs  at 40, 8 bytes per register in kernel order
 *   uc_mcontext.fpregs at 40 + 23*8 = 224
 *   uc_sigmask         at 296
 *   __fpregs_mem       at 424 (holds fnstenv + mxcsr image)
 * gregs indices: R8 0, R9 1, R10 2, R11 3, R12 4, R13 5, R14 6, R15 7,
 *   RDI 8, RSI 9, RBP 10, RBX 11, RDX 12, RAX 13, RCX 14, RSP 15, RIP 16
 *
 * getcontext saves the callee-saved registers, the caller's stack pointer
 * and return address, the floating-point control state and the signal
 * mask; resuming it makes getcontext return 0.  setcontext also loads the
 * argument registers, which makecontext uses to pass up to six arguments.
 */
#define G(r) (40 + 8 * (r))
#define FPREGS 224
#define SIGMASK 296
#define FPMEM 424

	.text
	.global getcontext
	.type getcontext,@function
getcontext:
	mov %rbx, G(11)(%rdi)
	mov %rbp, G(10)(%rdi)
	mov %r12, G(4)(%rdi)
	mov %r13, G(5)(%rdi)
	mov %r14, G(6)(%rdi)
	mov %r15, G(7)(%rdi)
	movq $0, G(13)(%rdi)          /* rax: resumed getcontext returns 0 */
	lea 8(%rsp), %rax
	mov %rax, G(15)(%rdi)         /* rsp after return */
	mov (%rsp), %rax
	mov %rax, G(16)(%rdi)         /* rip = return address */
	lea FPMEM(%rdi), %rax
	mov %rax, FPREGS(%rdi)
	fnstenv (%rax)
	fldenv (%rax)                 /* fnstenv masked x87 exceptions: undo */
	stmxcsr 28(%rax)
	/* rt_sigprocmask(SIG_BLOCK, NULL, &uc_sigmask, 8) */
	lea SIGMASK(%rdi), %rdx
	xor %esi, %esi
	xor %edi, %edi
	mov $8, %r10d
	mov $14, %eax
	syscall
	cmp $-4095, %rax
	jae 1f
	xor %eax, %eax
	ret
1:	neg %eax
	mov %eax, %edi
	jmp __ucontext_fail

	.size getcontext,.-getcontext

	.global setcontext
	.type setcontext,@function
setcontext:
	mov %rdi, %r12                /* preserved across the syscall */
	/* rt_sigprocmask(SIG_SETMASK, &uc_sigmask, NULL, 8) */
	lea SIGMASK(%rdi), %rsi
	xor %edx, %edx
	mov $2, %edi
	mov $8, %r10d
	mov $14, %eax
	syscall
	cmp $-4095, %rax
	jae 2f
	mov %r12, %rdx
	mov FPREGS(%rdx), %rax
	test %rax, %rax
	jz 1f
	fldenv (%rax)
	ldmxcsr 28(%rax)
1:	mov G(15)(%rdx), %rsp
	mov G(16)(%rdx), %rax
	push %rax                     /* resume address */
	mov G(11)(%rdx), %rbx
	mov G(10)(%rdx), %rbp
	mov G(4)(%rdx), %r12
	mov G(5)(%rdx), %r13
	mov G(6)(%rdx), %r14
	mov G(7)(%rdx), %r15
	mov G(8)(%rdx), %rdi
	mov G(9)(%rdx), %rsi
	mov G(14)(%rdx), %rcx
	mov G(0)(%rdx), %r8
	mov G(1)(%rdx), %r9
	mov G(13)(%rdx), %rax
	mov G(12)(%rdx), %rdx
	ret
2:	neg %eax
	mov %eax, %edi
	jmp __ucontext_fail
	.size setcontext,.-setcontext

	.global swapcontext
	.type swapcontext,@function
swapcontext:
	/* save into oucp (rdi) exactly like getcontext, resuming after us */
	push %rsi
	push %rdi
	sub $8, %rsp
	call getcontext
	add $8, %rsp
	pop %rdi
	pop %rsi
	test %eax, %eax
	jnz 1f
	/* fix up the saved rsp/rip so resuming returns from swapcontext */
	lea 8(%rsp), %rax
	mov %rax, G(15)(%rdi)
	mov (%rsp), %rax
	mov %rax, G(16)(%rdi)
	mov %rsi, %rdi
	jmp setcontext
1:	ret
	.size swapcontext,.-swapcontext

	/* entry functions made by makecontext return here: continue with
	 * uc_link (kept in rbx) or exit the process if there is none */
	.global __ucontext_trampoline
	.hidden __ucontext_trampoline
	.type __ucontext_trampoline,@function
__ucontext_trampoline:
	mov %rbx, %rdi
	test %rdi, %rdi
	jz 1f
	call setcontext
1:	xor %edi, %edi
	call exit
	hlt
	.size __ucontext_trampoline,.-__ucontext_trampoline

	.section .note.GNU-stack,"",@progbits
