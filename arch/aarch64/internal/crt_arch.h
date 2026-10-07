/*
 * lib-spfxd — AArch64 process entry point.
 *
 * The kernel enters _start with sp -> argc, argv[], NULL, envp[], NULL,
 * auxv (16-byte aligned) and x0 holding a finalizer the dynamic linker
 * wants registered (lib-spfxd's loader handles its own finalization, so it
 * is ignored).  The frame pointer and link register are cleared to end
 * backtraces; the original sp and the address of _DYNAMIC (0 in a static,
 * non-PIE executable) are passed to C.
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
	".type " START ",%function\n"
	START ":\n"
	"	mov x29, #0\n"
	"	mov x30, #0\n"
	"	mov x0, sp\n"
	"	adrp x1, _DYNAMIC\n"
	"	add x1, x1, #:lo12:_DYNAMIC\n"
	"	and sp, x0, #-16\n"
	"	b " START "_c\n"
	".size " START ", .-" START "\n"
);
