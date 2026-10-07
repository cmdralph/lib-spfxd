/* lib-spfxd — memccpy: copy up to and including the first c. */
#include <string.h>

void *memccpy(void *restrict dest, const void *restrict src, int c, size_t n)
{
	const unsigned char *hit = memchr(src, c, n);
	if (hit) {
		size_t len = (size_t)(hit - (const unsigned char *)src) + 1;
		memcpy(dest, src, len);
		return (unsigned char *)dest + len;
	}
	memcpy(dest, src, n);
	return 0;
}
