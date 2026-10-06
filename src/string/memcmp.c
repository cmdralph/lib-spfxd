/* lib-spfxd — memcmp: compares words while equal, then locates the first
 * differing byte.  Bytes compare as unsigned char. */
#include <string.h>
#include "string_impl.h"

int memcmp(const void *vl, const void *vr, size_t n)
{
	const unsigned char *l = vl, *r = vr;
	for (; n >= WSIZE; n -= WSIZE, l += WSIZE, r += WSIZE) {
		size_t a, b;
		__builtin_memcpy(&a, l, WSIZE);
		__builtin_memcpy(&b, r, WSIZE);
		if (a != b) {
			size_t i = (size_t)__builtin_ctzl(a ^ b) >> 3;
			return l[i] - r[i];
		}
	}
	for (; n; n--, l++, r++)
		if (*l != *r) return *l - *r;
	return 0;
}

