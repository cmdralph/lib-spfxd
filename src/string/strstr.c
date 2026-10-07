/* lib-spfxd — strstr / memmem / strcasestr front ends. */
#include <string.h>
#include "string_impl.h"


char *strstr(const char *h, const char *n)
{
	if (!n[0]) return (char *)h;
	h = strchr(h, n[0]);
	if (!h || !n[1]) return (char *)h;
	return __substr_search((const unsigned char *)h, 0, 1,
		(const unsigned char *)n, strlen(n), 0);
}

void *memmem(const void *h, size_t hl, const void *n, size_t nl)
{
	if (!nl) return (void *)h;
	if (hl < nl) return 0;
	const unsigned char *p = memchr(h, *(const unsigned char *)n, hl - nl + 1);
	if (!p || nl == 1) return (void *)p;
	hl -= (size_t)(p - (const unsigned char *)h);
	return __substr_search(p, hl, 0, n, nl, 0);
}

char *strcasestr(const char *h, const char *n)
{
	if (!n[0]) return (char *)h;
	return __substr_search((const unsigned char *)h, 0, 1,
		(const unsigned char *)n, strlen(n), 1);
}
