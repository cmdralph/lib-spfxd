/* lib-spfxd — fwrite / fputs / puts. */
#include <string.h>
#include "stdio_impl.h"

size_t fwrite(const void *restrict src, size_t size, size_t nmemb, FILE *restrict f)
{
	size_t len, k;
	if (!size || !nmemb) return 0;
	if (__builtin_mul_overflow(size, nmemb, &len)) len = (size_t)-1;
	FLOCK(f);
	if (!f->mode) f->mode = -1;
	k = __fwritex(src, len, f);
	FUNLOCK(f);
	return k == len ? nmemb : k / size;
}

size_t fwrite_unlocked(const void *restrict src, size_t size, size_t nmemb, FILE *restrict f)
{
	size_t len, k;
	if (!size || !nmemb) return 0;
	if (__builtin_mul_overflow(size, nmemb, &len)) len = (size_t)-1;
	if (!f->mode) f->mode = -1;
	k = __fwritex(src, len, f);
	return k == len ? nmemb : k / size;
}

int fputs(const char *restrict s, FILE *restrict f)
{
	size_t l = strlen(s);
	FLOCK(f);
	if (!f->mode) f->mode = -1;
	size_t k = __fwritex((const unsigned char *)s, l, f);
	FUNLOCK(f);
	return k == l ? 0 : EOF;
}
weak_alias(fputs, fputs_unlocked);

int puts(const char *s)
{
	FILE *f = &__stdout_FILE;
	size_t l = strlen(s);
	int r = 0;
	FLOCK(f);
	if (!f->mode) f->mode = -1;
	if (__fwritex((const unsigned char *)s, l, f) != l || putc_fast('\n', f) < 0) r = EOF;
	FUNLOCK(f);
	return r;
}
