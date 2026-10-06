/* lib-spfxd — strlcpy (BSD): returns strlen(s); truncates safely. */
#include <string.h>

size_t strlcpy(char *d, const char *s, size_t n)
{
	size_t len = strlen(s);
	if (n) {
		size_t k = len < n - 1 ? len : n - 1;
		memcpy(d, s, k);
		d[k] = 0;
	}
	return len;
}
