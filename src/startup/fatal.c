/* lib-spfxd — last-resort diagnostics that must not depend on stdio. */
#include <signal.h>
#include <stdlib.h>
#include "libc.h"
#include "syscall.h"

hidden size_t __libc_strlen_safe(const char *s)
{
	size_t n = 0;
	while (s[n]) n++;
	return n;
}

hidden void __libc_fatal(const char *msg)
{
	const char *pn = __libc.progname ? __libc.progname : "";
	__syscall(SYS_write, 2, pn, __libc_strlen_safe(pn));
	__syscall(SYS_write, 2, ": ", 2);
	__syscall(SYS_write, 2, msg, __libc_strlen_safe(msg));
	__syscall(SYS_write, 2, "\n", 1);
	abort();
}
