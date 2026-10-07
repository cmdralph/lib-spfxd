/* lib-spfxd — clone(): raw thread/process creation for applications. */
#include <sched.h>
#include <stdarg.h>
#include <errno.h>
#include "pthread_impl.h"

int clone(int (*fn)(void *), void *stack, int flags, void *arg, ...)
{
	va_list ap;
	va_start(ap, arg);
	pid_t *ptid = va_arg(ap, pid_t *);
	void *tls = va_arg(ap, void *);
	pid_t *ctid = va_arg(ap, pid_t *);
	va_end(ap);
	return (int)__syscall_ret((unsigned long)__clone(fn, stack, flags, arg, ptid, tls, ctid));
}
