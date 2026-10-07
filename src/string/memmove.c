/* lib-spfxd — memmove (portable): forward copy when dest precedes src or the
 * regions do not overlap, otherwise backward. */
#include <string.h>
#include "string_impl.h"

void *memmove(void *dest, const void *src, size_t n)
{
	unsigned char *d = dest;
	const unsigned char *s = src;

	if (d == s || !n) return dest;
	if ((uintptr_t)d - (uintptr_t)s >= n) {
		/* d < s, or no overlap: forward is safe */
		if ((uintptr_t)s - (uintptr_t)d >= n) return memcpy(d, s, n);
		if (((uintptr_t)s ^ (uintptr_t)d) % WSIZE == 0) {
			for (; !ALIGNED(d); n--) { if (!n) return dest; *d++ = *s++; }
			for (; n >= WSIZE; n -= WSIZE, d += WSIZE, s += WSIZE)
				*(word_t *)d = *(const word_t *)s;
		}
		for (; n; n--) *d++ = *s++;
	} else {
		d += n;
		s += n;
		if (((uintptr_t)s ^ (uintptr_t)d) % WSIZE == 0) {
			for (; !ALIGNED(d); n--) { if (!n) return dest; *--d = *--s; }
			for (; n >= WSIZE; n -= WSIZE) {
				d -= WSIZE;
				s -= WSIZE;
				*(word_t *)d = *(const word_t *)s;
			}
		}
		for (; n; n--) *--d = *--s;
	}
	return dest;
}
