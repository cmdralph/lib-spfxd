/*
 * lib-spfxd — memcpy (portable).  x86-64 builds use arch/x86_64/src/string/memcpy.S.
 *
 * Copies a byte prefix until the destination is word aligned, then whole
 * words.  When the source is misaligned relative to the destination the
 * words are assembled with shifts from aligned loads, so every load and
 * store is aligned regardless of the arguments.
 */
#include <string.h>
#include "string_impl.h"

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
	unsigned char *d = dest;
	const unsigned char *s = src;

	for (; n && !ALIGNED(d); n--) *d++ = *s++;

	if (ALIGNED(s)) {
		for (; n >= 4 * WSIZE; n -= 4 * WSIZE, d += 4 * WSIZE, s += 4 * WSIZE) {
			((word_t *)d)[0] = ((const word_t *)s)[0];
			((word_t *)d)[1] = ((const word_t *)s)[1];
			((word_t *)d)[2] = ((const word_t *)s)[2];
			((word_t *)d)[3] = ((const word_t *)s)[3];
		}
		for (; n >= WSIZE; n -= WSIZE, d += WSIZE, s += WSIZE)
			*(word_t *)d = *(const word_t *)s;
	} else if (n >= 2 * WSIZE) {
		/* little-endian merge of two aligned source words */
		unsigned shift = (unsigned)((uintptr_t)s & (WSIZE - 1)) * 8;
		const word_t *ws = (const word_t *)(s - ((uintptr_t)s & (WSIZE - 1)));
		size_t lo = *ws++;
		for (; n >= 2 * WSIZE; n -= WSIZE, d += WSIZE, s += WSIZE) {
			size_t hi = *ws++;
			*(word_t *)d = (lo >> shift) | (hi << (8 * WSIZE - shift));
			lo = hi;
		}
	}
	for (; n; n--) *d++ = *s++;
	return dest;
}
