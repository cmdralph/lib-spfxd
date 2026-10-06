/* lib-spfxd — translate a raw kernel return value into the C convention. */
#include <errno.h>
#include "syscall.h"

hidden long __syscall_ret(unsigned long r)
{
	if (r > -4096UL) {
		errno = (int)-r;
		return -1;
	}
	return (long)r;
}
