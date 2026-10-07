/* lib-spfxd — asprintf / vasprintf: format once into a stack buffer and
 * only format a second time when the result does not fit. */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "stdio_impl.h"

int vasprintf(char **s, const char *fmt, va_list ap)
{
	char small[256];
	va_list ap2;
	va_copy(ap2, ap);
	int l = vsnprintf(small, sizeof small, fmt, ap2);
	va_end(ap2);
	if (l < 0) return -1;
	char *p = malloc((size_t)l + 1);
	if (!p) return -1;
	if ((size_t)l < sizeof small) memcpy(p, small, (size_t)l + 1);
	else vsnprintf(p, (size_t)l + 1, fmt, ap);
	*s = p;
	return l;
}

int asprintf(char **s, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = vasprintf(s, fmt, ap);
	va_end(ap);
	return r;
}
