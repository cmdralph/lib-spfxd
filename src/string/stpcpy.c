/* lib-spfxd — stpcpy: word copy once the source is aligned, stopping at the
 * word containing the terminator. */
#include <string.h>
#include "string_impl.h"

char *stpcpy(char *restrict d, const char *restrict s)
{
	if (((uintptr_t)s ^ (uintptr_t)d) % WSIZE == 0) {
		for (; !ALIGNED(s); s++, d++)
			if (!(*d = *s)) return d;
		word_t *wd = (word_t *)d;
		const word_t *ws = (const word_t *)s;
		for (; !HASZERO(*ws); *wd++ = *ws++);
		d = (char *)wd;
		s = (const char *)ws;
	}
	for (; (*d = *s); s++, d++);
	return d;
}
