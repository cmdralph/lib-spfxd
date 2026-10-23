/* lib-spfxd — stpcpy (AArch64): the NEON strlen and memcpy are faster
 * than a word-at-a-time copy loop for all but the shortest strings. */
#include <string.h>

char *stpcpy(char *restrict d, const char *restrict s)
{
	size_t n = strlen(s);
	memcpy(d, s, n + 1);
	return d + n;
}
