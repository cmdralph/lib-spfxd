/* lib-spfxd — mempcpy (GNU): memcpy returning the end of the destination. */
#include <string.h>

void *mempcpy(void *restrict dest, const void *restrict src, size_t n)
{
	return (char *)memcpy(dest, src, n) + n;
}
