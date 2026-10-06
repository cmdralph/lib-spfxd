/* lib-spfxd — memset (portable).  Short fills use overlapping head/tail
 * stores; long fills store aligned words. */
#include <string.h>
#include "string_impl.h"

void *memset(void *dest, int c, size_t n)
{
	unsigned char *d = dest;
	size_t w = ONES * (unsigned char)c;

	if (n < 2 * WSIZE) {
		for (; n; n--) *d++ = (unsigned char)c;
		return dest;
	}
	/* unaligned head and tail words, then aligned interior */
	__builtin_memcpy(d, &w, WSIZE);
	__builtin_memcpy(d + n - WSIZE, &w, WSIZE);
	unsigned char *end = d + n - WSIZE;
	d = (unsigned char *)(((uintptr_t)d + WSIZE) & -(uintptr_t)WSIZE);
	for (; d + 4 * WSIZE <= end; d += 4 * WSIZE) {
		((word_t *)d)[0] = w;
		((word_t *)d)[1] = w;
		((word_t *)d)[2] = w;
		((word_t *)d)[3] = w;
	}
	for (; d < end; d += WSIZE) *(word_t *)d = w;
	return dest;
}
