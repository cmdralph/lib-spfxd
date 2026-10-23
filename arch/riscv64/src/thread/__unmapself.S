/*
 * lib-spfxd — void __unmapself(void *base, size_t len)
 *
 * An exiting detached thread frees its own stack: after munmap nothing may
 * touch memory, so only registers are used until the thread exits.
 */
	.text
	.global __unmapself
	.hidden __unmapself
	.type __unmapself,%function
__unmapself:
	li a7, 215            /* munmap(a0=base, a1=len) */
	ecall
	li a0, 0
	li a7, 93             /* exit(0): this thread only */
	ecall
	unimp
	.size __unmapself,.-__unmapself

	.section .note.GNU-stack,"",%progbits
