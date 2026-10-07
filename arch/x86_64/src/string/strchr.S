/*
 * lib-spfxd — strchr (x86-64): strchrnul, then NULL unless the byte found
 * is the target (for c == 0 the terminator itself is the target).
 */
	.text
	.global strchr
	.type strchr,@function
	.p2align 5
strchr:
	mov %esi,%r8d
	call __strchrnul
	cmp %r8b,(%rax)
	je 1f
	xor %eax,%eax
1:	ret
	.size strchr,.-strchr
	.section .note.GNU-stack,"",@progbits
