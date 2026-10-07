/*
 * lib-spfxd — void __unmapself(void *base, size_t len)
 *
 * Used by an exiting detached thread to free its own stack.  After munmap
 * the stack no longer exists, so the code must not touch memory: it uses
 * only registers until the exit syscall.  The caller has already blocked
 * all signals and cleared its kernel clear-tid address.
 */
	.text
	.global __unmapself
	.hidden __unmapself
	.type __unmapself,@function
__unmapself:
	mov $11,%eax          /* munmap(rdi=base, rsi=len) */
	syscall
	xor %edi,%edi
	mov $60,%eax          /* exit(0): this thread only */
	syscall
	hlt
	.size __unmapself,.-__unmapself

	.section .note.GNU-stack,"",@progbits
