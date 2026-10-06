/* lib-spfxd — crti.o: prologues of the legacy _init/_fini functions whose
 * bodies other objects contribute to .init/.fini.  On entry rsp is 8 mod 16;
 * the push restores 16-byte alignment for anything called from the body. */
	.section .init,"ax",@progbits
	.global _init
	.type _init,@function
_init:
	push %rax

	.section .fini,"ax",@progbits
	.global _fini
	.type _fini,@function
_fini:
	push %rax

	.section .note.GNU-stack,"",@progbits
