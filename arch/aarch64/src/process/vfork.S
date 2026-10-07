/*
 * lib-spfxd — pid_t vfork(void)
 *
 * AArch64 has no vfork system call: clone(CLONE_VM | CLONE_VFORK |
 * SIGCHLD, 0) shares memory and suspends the parent until the child execs
 * or exits.  The return address stays in x30 (a register), so the child
 * cannot clobber it on the shared stack.
 */
	.text
	.global vfork
	.type vfork,%function
	.hidden __syscall_ret
vfork:
	mov x0, #0x4111       /* CLONE_VM 0x100 | CLONE_VFORK 0x4000 | SIGCHLD */
	mov x1, #0
	mov x8, #220
	svc #0
	b __syscall_ret
	.size vfork,.-vfork

	.section .note.GNU-stack,"",%progbits
