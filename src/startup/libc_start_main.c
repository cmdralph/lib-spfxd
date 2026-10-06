/*
 * lib-spfxd — process startup.
 *
 * _start (crt1.o) -> _start_c -> __libc_start_main -> main
 *
 * Static executables:  __init_libc parses the initial stack (environment,
 *   auxiliary vector), installs the initial thread's TLS and thread pointer
 *   and the stack-protector canary.
 * Dynamic executables: the dynamic linker has already done all of that
 *   (and run the libraries' constructors) and set __libc.initialized.
 *
 * Then the executable's own destructors are registered (so that they run
 * after every atexit handler registered later), its constructors run, and
 * main's return value is passed to exit().
 */
#include <elf.h>
#include <poll.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "libc.h"
#include "pthread_impl.h"

hidden struct libc_state __libc;

char **__environ = 0;
weak_alias(__environ, ___environ);
weak_alias(__environ, _environ);
weak_alias(__environ, environ);

char *program_invocation_name = 0, *program_invocation_short_name = 0;

#define AUX_CNT 52


hidden void __init_libc(char **envp, char *pn)
{
	size_t i, *auxv, aux[AUX_CNT];

	for (i = 0; i < AUX_CNT; i++) aux[i] = 0;
	__environ = envp;
	for (i = 0; envp[i]; i++);
	__libc.auxv = auxv = (size_t *)(envp + i + 1);
	for (i = 0; auxv[i]; i += 2)
		if (auxv[i] < AUX_CNT) aux[auxv[i]] = auxv[i + 1];
	__libc.hwcap = aux[AT_HWCAP];
	__libc.page_size = aux[AT_PAGESZ] ? aux[AT_PAGESZ] : ARCH_PAGE_SIZE;

	if (!pn) pn = (char *)aux[AT_EXECFN];
	if (!pn) pn = (char *)"";
	program_invocation_name = pn;
	program_invocation_short_name = pn;
	for (i = 0; pn[i]; i++)
		if (pn[i] == '/') program_invocation_short_name = pn + i + 1;
	__libc.progname = program_invocation_short_name;

	__init_tls(aux);
	__init_ssp((void *)aux[AT_RANDOM]);

	if (aux[AT_UID] == aux[AT_EUID] && aux[AT_GID] == aux[AT_EGID] && !aux[AT_SECURE])
		return;

	/* Privileged (setuid/setgid) start: never let the standard descriptors
	 * be closed, or a later open() could become stdout/stderr. */
	struct pollfd pfd[3] = { { .fd = 0 }, { .fd = 1 }, { .fd = 2 } };
	if (__syscall(SYS_poll, pfd, 3, 0) < 0) a_crash();
	for (i = 0; i < 3; i++)
		if ((pfd[i].revents & POLLNVAL) &&
		    __syscall(SYS_open, "/dev/null", O_RDWR) < 0)
			a_crash();
	__libc.secure = 1;
}

static void call_fini(void *f)
{
	((void (*)(void))f)();
}

/* Kept out of line so that nothing touching TLS (errno, the canary) can be
 * scheduled before the thread pointer exists. */
static noinline int start_main_stage2(int (*main)(int, char **, char **), int argc,
	char **argv, void (*init)(void), void (*fini)(void))
{
	char **envp = argv + argc + 1;
	if (fini) __cxa_atexit(call_fini, (void *)fini, 0);
	if (init) init();
	exit(main(argc, argv, envp));
}

int __libc_start_main(int (*main)(int, char **, char **), int argc, char **argv,
	void (*init)(void), void (*fini)(void), void (*ldso_fini)(void))
{
	if (!__libc.initialized) {
		__init_libc(argv + argc + 1, argv[0]);
		__libc.initialized = 1;
	}
	/* Compiler barrier: stage 2 must observe the initialized runtime. */
	__asm__ __volatile__ ("" ::: "memory");
	return start_main_stage2(main, argc, argv, init, fini);
}
