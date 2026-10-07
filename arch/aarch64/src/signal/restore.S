/*
 * lib-spfxd — signal return trampoline (AArch64).
 *
 * Installed as sa_restorer for every handler (SA_RESTORER).  A returning
 * handler lands here with sp at the kernel-built signal frame;
 * rt_sigreturn restores the interrupted context from it.
 */
	.text
	.global __restore_rt
	.hidden __restore_rt
	.type __restore_rt,%function
__restore_rt:
	mov x8, #139
	svc #0
	udf #0
	.size __restore_rt,.-__restore_rt

	.section .note.GNU-stack,"",%progbits
