/*
 * lib-spfxd — x86-64 process entry point.
 *
 * The kernel enters _start with:
 *   rsp -> argc, argv[0..argc-1], NULL, envp[...], NULL, auxv pairs, AT_NULL
 *   rdx -> a finalizer the dynamic linker wants registered (0 for static
 *          executables; lib-spfxd's loader handles its own finalization, so
 *          it is ignored)
 *   all other registers unspecified, except that rsp is 16-byte aligned.
 *
 * We clear rbp to terminate frame-pointer based backtraces, pass the
 * original stack pointer to C, and re-align rsp to 16 bytes so the `call`
 * leaves the stack exactly as the SysV ABI requires on function entry.
 */
#ifndef START_EXTRA
#define START_EXTRA ""
#endif
__asm__(
	".text\n"
	START_EXTRA
	".weak _DYNAMIC\n"
	".hidden _DYNAMIC\n"
	".global " START "\n"
	".type " START ",@function\n"
	START ":\n"
	"	xor %ebp,%ebp\n"
	"	mov %rsp,%rdi\n"
	"	lea _DYNAMIC(%rip),%rsi\n"
	"	and $-16,%rsp\n"
	"	call " START "_c\n"
	"	hlt\n"
	".size " START ", .-" START "\n"
);
