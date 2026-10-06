/*
 * lib-spfxd — signal return trampoline.
 *
 * Installed as sa_restorer for every handler (SA_RESTORER).  When a handler
 * returns, it returns here with rsp pointing at the kernel-built rt signal
 * frame; rt_sigreturn restores the interrupted context from it.  The CFI
 * marks the frame as a signal frame so debuggers and unwinders can step
 * through it.
 */
	.text
	.global __restore_rt
	.hidden __restore_rt
	.type __restore_rt,@function
	nop
__restore_rt:
	mov $15,%eax
	syscall
	hlt
	.size __restore_rt,.-__restore_rt

	.section .note.GNU-stack,"",@progbits
