/*
 * lib-spfxd — internal <errno.h> wrapper.
 *
 * Library sources see this header instead of the public one (src/internal
 * precedes include/ in the search path).  errno lives in the thread control
 * block at a fixed offset, so inside the library it is accessed directly
 * through the %fs segment: a single instruction, no function call.
 */
#include_next <errno.h>

#ifndef _SPFXD_INTERNAL_ERRNO_H
#define _SPFXD_INTERNAL_ERRNO_H
#include "arch.h"

#define SPFXD_TCB_ERRNO_OFFSET 0x3c

#undef errno
#if TLS_ABOVE_TP
/* variant I: struct pthread sits below the thread pointer at a distance
 * this header cannot see; go through __errno_location (declared const, so
 * repeated uses fold) */
int *__errno_location(void) __attribute__((__const__));
#define errno (*__errno_location())
#elif defined(__SEG_FS) && !defined(SPFXD_NO_SEG_FS)
#define errno (*(int __seg_fs *)SPFXD_TCB_ERRNO_OFFSET)
#else
#define errno (*(int *)((char *)__arch_tp() + SPFXD_TCB_ERRNO_OFFSET))
#endif
#endif
