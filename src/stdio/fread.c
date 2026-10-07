/*
 * lib-spfxd — fread.  Buffered data is consumed first; a remaining request
 * at least as large as the buffer is read straight into the caller's
 * memory, smaller ones go through the buffer.
 */
#include <string.h>
#include "stdio_impl.h"

static size_t fread_unlocked_impl(unsigned char *dest, size_t len, FILE *f)
{
	size_t l = len, k;
	if (!f->mode) f->mode = -1;
	if (f->rpos != f->rend) {
		k = (size_t)(f->rend - f->rpos);
		if (k > l) k = l;
		memcpy(dest, f->rpos, k);
		f->rpos += k;
		dest += k;
		l -= k;
	}
	while (l) {
		if (__toread(f)) break;
		__stdin_refill_hook(f);
		if (l >= f->buf_size) {
			k = f->read(f, dest, l);
		} else {
			k = f->read(f, f->buf, f->buf_size);
			f->rpos = f->buf;
			f->rend = f->buf + k;
			if (k > l) k = l;
			memcpy(dest, f->rpos, k);
			f->rpos += k;
		}
		if (!k) break;
		dest += k;
		l -= k;
	}
	return len - l;
}

size_t fread(void *restrict destv, size_t size, size_t nmemb, FILE *restrict f)
{
	size_t len;
	if (!size || !nmemb) return 0;
	if (__builtin_mul_overflow(size, nmemb, &len)) len = (size_t)-1;
	FLOCK(f);
	size_t got = fread_unlocked_impl(destv, len, f);
	FUNLOCK(f);
	return got == len ? nmemb : got / size;
}

size_t fread_unlocked(void *restrict destv, size_t size, size_t nmemb, FILE *restrict f)
{
	size_t len;
	if (!size || !nmemb) return 0;
	if (__builtin_mul_overflow(size, nmemb, &len)) len = (size_t)-1;
	size_t got = fread_unlocked_impl(destv, len, f);
	return got == len ? nmemb : got / size;
}
