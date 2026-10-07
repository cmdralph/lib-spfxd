/* lib-spfxd — stpncpy. */
#include <string.h>

char *stpncpy(char *restrict d, const char *restrict s, size_t n)
{
	size_t len = strnlen(s, n);
	memcpy(d, s, len);
	memset(d + len, 0, n - len);
	return d + len;
}
