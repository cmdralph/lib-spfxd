/* lib-spfxd — crti.o (RISC-V 64): prologues of _init and _fini; crtn.o
 * holds the matching epilogues, with the .init/.fini fragments of other
 * objects linked in between. */
	.section .init,"ax",%progbits
	.global _init
	.type _init,%function
_init:
	addi sp, sp, -16
	sd ra, 8(sp)

	.section .fini,"ax",%progbits
	.global _fini
	.type _fini,%function
_fini:
	addi sp, sp, -16
	sd ra, 8(sp)

	.section .note.GNU-stack,"",%progbits
