/* lib-spfxd — long syscall(long nr, ...): generic system call entry. */
#include <unistd.h>
#include <stdarg.h>
#include "syscall.h"

long syscall(long nr, ...)
{
	va_list ap;
	long a, b, c, d, e, f;
	va_start(ap, nr);
	a = va_arg(ap, long);
	b = va_arg(ap, long);
	c = va_arg(ap, long);
	d = va_arg(ap, long);
	e = va_arg(ap, long);
	f = va_arg(ap, long);
	va_end(ap);
	return __syscall_ret((unsigned long)__syscall6(nr, a, b, c, d, e, f));
}
