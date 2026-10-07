/* lib-spfxd — crti.o (AArch64): prologues of _init and _fini.  The linker
 * concatenates the .init/.fini fragments of all objects between crti.o and
 * crtn.o; crtn.o supplies the matching epilogues. */
	.section .init,"ax",%progbits
	.global _init
	.type _init,%function
_init:
	stp x29, x30, [sp, #-16]!
	mov x29, sp

	.section .fini,"ax",%progbits
	.global _fini
	.type _fini,%function
_fini:
	stp x29, x30, [sp, #-16]!
	mov x29, sp

	.section .note.GNU-stack,"",%progbits
