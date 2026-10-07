/* lib-spfxd — printf / fprintf / vprintf. */
#include <stdarg.h>
#include "stdio_impl.h"

int printf(const char *restrict fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = vfprintf(&__stdout_FILE, fmt, ap);
	va_end(ap);
	return r;
}

int fprintf(FILE *restrict f, const char *restrict fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = vfprintf(f, fmt, ap);
	va_end(ap);
	return r;
}

int vprintf(const char *restrict fmt, va_list ap)
{
	return vfprintf(&__stdout_FILE, fmt, ap);
}
