/*
 * lib-spfxd — sprintf family.  The formatter writes directly into the
 * caller's array: a private FILE whose buffer *is* the destination.  Once
 * the array is full, further output is counted but discarded.
 */
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include "stdio_impl.h"

static size_t sn_write(FILE *f, const unsigned char *s, size_t l)
{
	size_t room = (size_t)(f->wend - f->wpos);
	size_t k = l < room ? l : room;
	memcpy(f->wpos, s, k);
	f->wpos += k;
	f->wend = f->wpos;
	return l;
}

hidden void __string_file_init(FILE *f, unsigned char *buf, size_t size)
{
	memset(f, 0, sizeof *f);
	f->flags = F_NOLOCK | F_PERM;
	f->fd = -1;
	f->lbf = EOF;
	f->buf = buf;
	f->buf_size = f->buf_cap = size;
	f->mode = -1;
}

int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap)
{
	FILE f;
	unsigned char dummy[1];
	unsigned char *dst = n ? (unsigned char *)s : dummy;
	size_t cap = n ? n - 1 : 0;
	__string_file_init(&f, dst, cap);
	f.flags |= F_NORD;
	f.write = sn_write;
	f.wbase = f.wpos = dst;
	f.wend = dst + cap;
	int r = __vfprintf_core(&f, fmt, ap);
	if (n) *f.wpos = 0;
	return r;
}

int snprintf(char *restrict s, size_t n, const char *restrict fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = vsnprintf(s, n, fmt, ap);
	va_end(ap);
	return r;
}

int vsprintf(char *restrict s, const char *restrict fmt, va_list ap)
{
	size_t lim = SIZE_MAX - (uintptr_t)s;
	if (lim > (size_t)INT_MAX + 1) lim = (size_t)INT_MAX + 1;
	return vsnprintf(s, lim, fmt, ap);
}

int sprintf(char *restrict s, const char *restrict fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = vsprintf(s, fmt, ap);
	va_end(ap);
	return r;
}
