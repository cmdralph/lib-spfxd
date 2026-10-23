/*
 * lib-spfxd — pid_t vfork(void)
 *
 * RISC-V has no vfork system call: clone(CLONE_VM | CLONE_VFORK |
 * SIGCHLD, 0) shares memory and suspends the parent until the child execs
 * or exits.  The return address stays in ra (a register), so the child
 * cannot clobber it on the shared stack.
 */
	.text
	.global vfork
	.type vfork,%function
	.hidden __syscall_ret
vfork:
	li a0, 0x4111         /* CLONE_VM 0x100 | CLONE_VFORK 0x4000 | SIGCHLD */
	li a1, 0
	li a7, 220
	ecall
	tail __syscall_ret
	.size vfork,.-vfork

	.section .note.GNU-stack,"",%progbits
