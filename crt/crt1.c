/*
 * lib-spfxd — crt1.o / Scrt1.o: the executable's entry point.
 *
 * This object is linked into every executable.  Besides _start it owns the
 * executable's own constructor/destructor tables (.preinit_array,
 * .init_array, .fini_array, legacy .init/.fini), which it hands to
 * __libc_start_main as two callbacks.  Keeping this logic here means the
 * same code works for static executables and for executables using libc.so,
 * where the library cannot see the executable's linker-defined symbols.
 */
#include <features.h>
#include "libc.h"

#ifndef START
#define START "_start"
#endif
#include "crt_arch.h"

int main(int, char **, char **);

extern void _init(void) __attribute__((__weak__));
extern void _fini(void) __attribute__((__weak__));
extern void (*const __preinit_array_start[])(int, char **, char **) __attribute__((__weak__, __visibility__("hidden")));
extern void (*const __preinit_array_end[])(int, char **, char **) __attribute__((__weak__, __visibility__("hidden")));
extern void (*const __init_array_start[])(int, char **, char **) __attribute__((__weak__, __visibility__("hidden")));
extern void (*const __init_array_end[])(int, char **, char **) __attribute__((__weak__, __visibility__("hidden")));
extern void (*const __fini_array_start[])(void) __attribute__((__weak__, __visibility__("hidden")));
extern void (*const __fini_array_end[])(void) __attribute__((__weak__, __visibility__("hidden")));

static int s_argc;
static char **s_argv;

/* Order mandated by the System V gABI: preinit array, DT_INIT, init array.
 * Constructors receive (argc, argv, envp) as GNU toolchains expect. */
static void run_init(void)
{
	char **envp = s_argv + s_argc + 1;
	size_t n = (size_t)(__preinit_array_end - __preinit_array_start);
	for (size_t i = 0; i < n; i++) __preinit_array_start[i](s_argc, s_argv, envp);
	if (_init) _init();
	n = (size_t)(__init_array_end - __init_array_start);
	for (size_t i = 0; i < n; i++) __init_array_start[i](s_argc, s_argv, envp);
}

/* Destructors run in reverse order of construction. */
static void run_fini(void)
{
	size_t n = (size_t)(__fini_array_end - __fini_array_start);
	while (n) __fini_array_start[--n]();
	if (_fini) _fini();
}

#ifndef START_C_ATTR
#define START_C_ATTR
void _start_c(long *, void *);
#endif

START_C_ATTR __attribute__((__used__, __noreturn__)) void _start_c(long *p, void *dyn)
{
#ifdef RCRT1_SELF_RELOCATE
	RCRT1_SELF_RELOCATE(p, dyn);
#endif
	s_argc = (int)p[0];
	s_argv = (char **)(p + 1);
	__libc_start_main(main, s_argc, s_argv, run_init, run_fini, 0);
	for (;;) __arch_crash();
}
