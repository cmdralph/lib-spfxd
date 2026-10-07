/* lib-spfxd — dprintf / vdprintf: printf to a file descriptor through a
 * stack-buffered private stream. */
#include <stdarg.h>
#include "stdio_impl.h"

int vdprintf(int fd, const char *restrict fmt, va_list ap)
{
	unsigned char buf[1024];
	FILE f;
	__string_file_init(&f, buf, sizeof buf);
	f.flags |= F_NORD;
	f.fd = fd;
	f.write = __stdio_write;
	int r = __vfprintf_core(&f, fmt, ap);
	if (f.wpos != f.wbase) f.write(&f, 0, 0);
	return (f.flags & F_ERR) ? -1 : r;
}

int dprintf(int fd, const char *restrict fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = vdprintf(fd, fmt, ap);
	va_end(ap);
	return r;
}
