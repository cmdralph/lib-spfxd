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
	mov x8, #215          /* munmap(x0=base, x1=len) */
	svc #0
	mov x0, #0
	mov x8, #93           /* exit(0): this thread only */
	svc #0
	udf #0
	.size __unmapself,.-__unmapself

	.section .note.GNU-stack,"",%progbits
