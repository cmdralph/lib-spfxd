/*
 * lib-spfxd — x86-64 TLS descriptor resolvers (R_X86_64_TLSDESC,
 * -mtls-dialect=gnu2).
 *
 * A descriptor is two words { resolver, argument }.  The compiler emits
 *     lea  var@TLSDESC(%rip), %rax
 *     call *var@TLSCALL(%rax)
 * and expects in %rax the variable's offset from the thread pointer
 * (%fs:0); every other register must be preserved (the flags need not).
 *
 * __tlsdesc_static   modules with static TLS: the argument is the offset.
 * __tlsdesc_dynamic  modules loaded by dlopen: the argument points to
 *                    { module id, offset }.  The fast path reads this
 *                    thread's dtv (%fs:8) directly; the first access from
 *                    a thread calls __tls_get_addr, which may allocate, so
 *                    the slow path saves the general registers and the
 *                    whole extended state (xsave, or fxsave without OS
 *                    XSAVE support).
 */
	.text
	.globl __tlsdesc_static
	.hidden __tlsdesc_static
	.type __tlsdesc_static, @function
__tlsdesc_static:
	movq 8(%rax), %rax
	ret
	.size __tlsdesc_static, . - __tlsdesc_static

	.globl __tlsdesc_dynamic
	.hidden __tlsdesc_dynamic
	.type __tlsdesc_dynamic, @function
__tlsdesc_dynamic:
	movq 8(%rax), %rax              /* ti = { module id, offset } */
	pushq %rdx
	pushq %rcx
	movq %fs:8, %rdx                /* dtv; dtv[0] = number of entries */
	movq (%rax), %rcx
	cmpq (%rdx), %rcx
	ja 1f
	movq (%rdx,%rcx,8), %rdx
	testq %rdx, %rdx
	jz 1f
	addq 8(%rax), %rdx
	subq %fs:0, %rdx
	movq %rdx, %rax
	popq %rcx
	popq %rdx
	ret

1:	/* slow path: %rcx and %rdx are already saved */
	pushq %rbp
	movq %rsp, %rbp
	pushq %rsi
	pushq %rdi
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %rbx
	movq %rax, %rdi
	movl $1, %eax
	cpuid
	btl $27, %ecx                   /* OSXSAVE */
	jnc 3f
	movl $0xd, %eax
	xorl %ecx, %ecx
	cpuid                           /* %ebx = save area size for XCR0 */
	subq %rbx, %rsp
	andq $-64, %rsp
	xorl %eax, %eax                 /* the XSAVE header must start zeroed */
	movq %rax, 512(%rsp)
	movq %rax, 520(%rsp)
	movq %rax, 528(%rsp)
	movq %rax, 536(%rsp)
	movq %rax, 544(%rsp)
	movq %rax, 552(%rsp)
	movq %rax, 560(%rsp)
	movq %rax, 568(%rsp)
	movl $-1, %eax
	movl $-1, %edx
	xsave64 (%rsp)
	call __tls_get_addr@PLT
	movq %rax, %rbx
	movl $-1, %eax
	movl $-1, %edx
	xrstor64 (%rsp)
	movq %rbx, %rax
	jmp 4f
3:	subq $512, %rsp
	andq $-16, %rsp
	fxsave64 (%rsp)
	call __tls_get_addr@PLT
	fxrstor64 (%rsp)
4:	subq %fs:0, %rax
	leaq -56(%rbp), %rsp
	popq %rbx
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rdi
	popq %rsi
	popq %rbp
	popq %rcx
	popq %rdx
	ret
	.size __tlsdesc_dynamic, . - __tlsdesc_dynamic

	.section .note.GNU-stack, "", @progbits
