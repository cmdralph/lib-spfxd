/* lib-spfxd — strncat: appends at most n bytes, always terminates. */
#include <string.h>

char *strncat(char *restrict d, const char *restrict s, size_t n)
{
	char *e = d + strlen(d);
	size_t len = strnlen(s, n);
	memcpy(e, s, len);
	e[len] = 0;
	return d;
}
