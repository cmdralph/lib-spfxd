/*
 * lib-spfxd — int __clone(int (*fn)(void *), void *stack, int flags,
 *                         void *arg, pid_t *ptid, void *tls, pid_t *ctid)
 *
 * C arguments:  rdi=fn rsi=stack rdx=flags rcx=arg r8=ptid r9=tls 8(%rsp)=ctid
 * Kernel clone: rax=56 rdi=flags rsi=newsp rdx=ptid r10=ctid r8=tls
 *
 * fn and arg are stored at the top of the child's stack because every
 * register except rax/rcx/r11 survives the syscall in the child, but the
 * child's stack is the only memory we know it can use.  The child stack is
 * aligned so that after popping fn and arg rsp is 16-byte aligned, as the
 * ABI requires before `call`.  The child never returns from here: it calls
 * fn and terminates the *thread* (SYS_exit, not exit_group) with its result.
 */
	.text
	.global __clone
	.hidden __clone
	.type __clone,@function
__clone:
	and $-16,%rsi
	sub $16,%rsi
	mov %rdi,0(%rsi)
	mov %rcx,8(%rsi)
	mov %rdx,%rdi
	mov %r8,%rdx
	mov %r9,%r8
	mov 8(%rsp),%r10
	mov $56,%eax
	syscall
	test %eax,%eax
	jnz 1f
	/* child */
	.cfi_startproc
	.cfi_undefined %rip
	xor %ebp,%ebp
	pop %rax
	pop %rdi
	call *%rax
	mov %eax,%edi
	mov $60,%eax
	syscall
	hlt
	.cfi_endproc
1:	ret
	.size __clone,.-__clone

	.section .note.GNU-stack,"",@progbits
