/*
 * lib-spfxd — dlerror and the per-thread error state of the dl* functions.
 *
 * Each thread has its own pending message (kept in its TCB), so a thread
 * never sees another thread's error.  dlerror returns the message once and
 * then reports no error until the next failure.
 */
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "pthread_impl.h"

hidden void __dl_seterr(const char *fmt, ...);

void __dl_seterr(const char *fmt, ...)
{
	struct pthread *self = __self();
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (n < 0) n = 0;
	char *m = realloc(self->dlerror_buf, (size_t)n + 1 < sizeof buf ? (size_t)n + 1 : sizeof buf);
	if (!m) {
		/* keep any previous buffer; report a generic message */
		self->dlerror_flag = 2;
		return;
	}
	snprintf(m, (size_t)n + 1 < sizeof buf ? (size_t)n + 1 : sizeof buf, "%s", buf);
	self->dlerror_buf = m;
	self->dlerror_flag = 1;
}

char *dlerror(void)
{
	struct pthread *self = __self();
	int f = self->dlerror_flag;
	self->dlerror_flag = 0;
	if (f == 2) return (char *)"dynamic linker error (out of memory for message)";
	return f ? self->dlerror_buf : 0;
}
