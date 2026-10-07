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
#if defined(__SEG_FS) && !defined(SPFXD_NO_SEG_FS)
#define errno (*(int __seg_fs *)SPFXD_TCB_ERRNO_OFFSET)
#else
#define errno (*(int *)((char *)__arch_tp() + SPFXD_TCB_ERRNO_OFFSET))
#endif
#endif
