/* lib-spfxd — memrchr (GNU): last occurrence of a byte. */
#include <string.h>
#include "string_impl.h"

void *memrchr(const void *src, int c, size_t n)
{
	const unsigned char *s = src;
	unsigned char ch = (unsigned char)c;
	while (n && !ALIGNED(s + n)) {
		if (s[--n] == ch) return (void *)(s + n);
	}
	size_t k = ONES * ch;
	while (n >= WSIZE) {
		size_t w = *(const word_t *)(s + n - WSIZE) ^ k;
		if (HASZERO(w)) break;
		n -= WSIZE;
	}
	while (n)
		if (s[--n] == ch) return (void *)(s + n);
	return 0;
}
