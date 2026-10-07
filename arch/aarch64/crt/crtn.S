/* lib-spfxd — crtn.o (AArch64): epilogues of _init and _fini. */
	.section .init,"ax",%progbits
	ldp x29, x30, [sp], #16
	ret

	.section .fini,"ax",%progbits
	ldp x29, x30, [sp], #16
	ret

	.section .note.GNU-stack,"",%progbits
