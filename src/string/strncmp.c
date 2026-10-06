/* lib-spfxd — strncmp. */
#include <string.h>

int strncmp(const char *l, const char *r, size_t n)
{
	const unsigned char *a = (const void *)l, *b = (const void *)r;
	if (!n) return 0;
	for (; --n && *a && *a == *b; a++, b++);
	return *a - *b;
}
