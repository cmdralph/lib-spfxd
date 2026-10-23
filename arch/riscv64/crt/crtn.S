/* lib-spfxd — crtn.o (RISC-V 64): epilogues of _init and _fini. */
	.section .init,"ax",%progbits
	ld ra, 8(sp)
	addi sp, sp, 16
	ret

	.section .fini,"ax",%progbits
	ld ra, 8(sp)
	addi sp, sp, 16
	ret

	.section .note.GNU-stack,"",%progbits
