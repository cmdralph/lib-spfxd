/* lib-spfxd — memchr: word-at-a-time search for a byte value. */
#include <string.h>
#include "string_impl.h"

void *memchr(const void *src, int c, size_t n)
{
	const unsigned char *s = src;
	unsigned char ch = (unsigned char)c;

	for (; !ALIGNED(s) && n; s++, n--)
		if (*s == ch) return (void *)s;
	if (n >= WSIZE) {
		size_t k = ONES * ch;
		for (; n >= WSIZE; s += WSIZE, n -= WSIZE) {
			size_t z = HASZERO(*(const word_t *)s ^ k);
			if (z) return (void *)(s + __zero_byte_index(z));
		}
	}
	for (; n; s++, n--)
		if (*s == ch) return (void *)s;
	return 0;
}
