/*
 * lib-spfxd — pid_t vfork(void)
 *
 * The child runs on the parent's stack until it execs or exits, so the
 * return address is held in a register across the syscall: if it stayed
 * on the stack the child could overwrite it before the parent resumes.
 */
	.text
	.global vfork
	.type vfork,@function
	.hidden __syscall_ret
vfork:
	pop %rdx
	mov $58,%eax
	syscall
	push %rdx
	mov %rax,%rdi
	jmp __syscall_ret
	.size vfork,.-vfork

	.section .note.GNU-stack,"",@progbits
