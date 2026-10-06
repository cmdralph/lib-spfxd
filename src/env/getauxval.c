/* lib-spfxd — getauxval. */
#include <sys/auxv.h>
#include <errno.h>
#include "libc.h"

unsigned long getauxval(unsigned long type)
{
	if (type == AT_SECURE) return (unsigned long)__libc.secure;
	for (size_t *a = __libc.auxv; a && *a; a += 2)
		if (*a == type) return a[1];
	errno = ENOENT;
	return 0;
}
