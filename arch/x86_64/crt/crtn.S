/* lib-spfxd — crtn.o: epilogues matching crti.o. */
	.section .init,"ax",@progbits
	pop %rax
	ret

	.section .fini,"ax",@progbits
	pop %rax
	ret

	.section .note.GNU-stack,"",@progbits
