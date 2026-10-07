/* lib-spfxd — strchrnul: first c or the terminator, scanning a word at a
 * time for either byte. */
#include <string.h>
#include "string_impl.h"

char *strchrnul(const char *s, int c)
{
	unsigned char ch = (unsigned char)c;
	if (!ch) return (char *)s + strlen(s);
	for (; !ALIGNED(s); s++)
		if (!*s || *(const unsigned char *)s == ch) return (char *)s;
	size_t k = ONES * ch;
	const word_t *w = (const word_t *)s;
	for (; !HASZERO(*w) && !HASZERO(*w ^ k); w++);
	for (s = (const char *)w; *s && *(const unsigned char *)s != ch; s++);
	return (char *)s;
}
