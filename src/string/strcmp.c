/* lib-spfxd — strcmp: word compare while both strings are co-aligned. */
#include <string.h>
#include "string_impl.h"

int strcmp(const char *l, const char *r)
{
	if (((uintptr_t)l ^ (uintptr_t)r) % WSIZE == 0) {
		for (; !ALIGNED(l); l++, r++)
			if (*l != *r || !*l) return (unsigned char)*l - (unsigned char)*r;
		const word_t *wl = (const word_t *)l, *wr = (const word_t *)r;
		for (; *wl == *wr && !HASZERO(*wl); wl++, wr++);
		l = (const char *)wl;
		r = (const char *)wr;
	}
	for (; *l == *r && *l; l++, r++);
	return (unsigned char)*l - (unsigned char)*r;
}
