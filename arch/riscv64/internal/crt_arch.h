/*
 * lib-spfxd — RISC-V 64 process entry point.
 *
 * The kernel enters _start with sp -> argc, argv[], NULL, envp[], NULL,
 * auxv and a0 holding a finalizer for the program to register (lib-spfxd's
 * loader handles its own finalization, so it is ignored).  A program's
 * _start first loads gp, the base of linker-relaxed accesses to small
 * data, with relaxation off for that one instruction pair; the dynamic
 * linker's entry (START_NO_GP) leaves gp to the program.  ra and the
 * frame pointer are cleared to end backtraces; the original sp and the
 * address of _DYNAMIC (0 in a static, non-PIE executable) are passed to C.
 */
#ifndef START_EXTRA
#define START_EXTRA ""
#endif
#ifdef START_NO_GP
#define START_GP ""
#else
#define START_GP ".option push\n.option norelax\n	lla gp, __global_pointer$\n.option pop\n"
#endif
__asm__(
	".text\n"
	START_EXTRA
	".weak _DYNAMIC\n"
	".hidden _DYNAMIC\n"
	".global " START "\n"
	".type " START ",%function\n"
	START ":\n"
	START_GP
	"	li ra, 0\n"
	"	li s0, 0\n"
	"	mv a0, sp\n"
	"	lla a1, _DYNAMIC\n"
	"	andi sp, sp, -16\n"
	"	tail " START "_c\n"
	".size " START ", .-" START "\n"
);
