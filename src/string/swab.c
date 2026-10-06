/* lib-spfxd — swab: copy swapping adjacent byte pairs. */
#include <unistd.h>

void swab(const void *restrict src, void *restrict dest, ssize_t n)
{
	const unsigned char *s = src;
	unsigned char *d = dest;
	for (; n > 1; n -= 2, s += 2, d += 2) {
		d[0] = s[1];
		d[1] = s[0];
	}
}
