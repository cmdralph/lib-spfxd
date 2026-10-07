/* lib-spfxd — rawmemchr (GNU): memchr without a length limit. */
#include <string.h>
#include <stdint.h>

void *rawmemchr(const void *s, int c)
{
	if (!c) return (char *)s + strlen(s);
	return memchr(s, c, SIZE_MAX - (uintptr_t)s);
}
