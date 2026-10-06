/* lib-spfxd — public access to the calling thread's errno. */
#include <errno.h>
#include "pthread_impl.h"

int *__errno_location(void)
{
	return &__self()->errno_val;
}
