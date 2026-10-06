/* lib-spfxd — sigaltstack. */
#include <errno.h>
#include <signal.h>
#include "syscall.h"

int sigaltstack(const stack_t *restrict ss, stack_t *restrict old)
{
	if (ss) {
		if (!(ss->ss_flags & SS_DISABLE) && ss->ss_size < MINSIGSTKSZ) {
			errno = ENOMEM;
			return -1;
		}
		if (ss->ss_flags & ~(SS_DISABLE | SS_AUTODISARM)) {
			errno = EINVAL;
			return -1;
		}
	}
	return (int)__sysret(SYS_sigaltstack, ss, old);
}
