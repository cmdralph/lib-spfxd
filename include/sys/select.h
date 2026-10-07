/* lib-spfxd — <sys/select.h> */
#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H
#include <features.h>

#define __SPFXD_NEED_time_t
#define __SPFXD_NEED_suseconds_t
#define __SPFXD_NEED_struct_timeval
#define __SPFXD_NEED_struct_timespec
#define __SPFXD_NEED_sigset_t
#include <bits/typedefs.h>

#define FD_SETSIZE 1024
typedef unsigned long fd_mask;
typedef struct { unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(long))]; } fd_set;

#define __FD_WORD(d) ((d) / (8 * sizeof(long)))
#define __FD_BIT(d)  (1UL << ((d) % (8 * sizeof(long))))
#define FD_ZERO(s)   __builtin_memset((s), 0, sizeof(fd_set))
#define FD_SET(d, s)   ((void)((s)->fds_bits[__FD_WORD(d)] |= __FD_BIT(d)))
#define FD_CLR(d, s)   ((void)((s)->fds_bits[__FD_WORD(d)] &= ~__FD_BIT(d)))
#define FD_ISSET(d, s) (!!((s)->fds_bits[__FD_WORD(d)] & __FD_BIT(d)))
#if defined(__SPFXD_BSD)
#define NFDBITS (8 * (int)sizeof(long))
#endif

__SPFXD_BEGIN_DECLS
int select(int, fd_set *__restrict, fd_set *__restrict, fd_set *__restrict, struct timeval *__restrict);
int pselect(int, fd_set *__restrict, fd_set *__restrict, fd_set *__restrict,
	const struct timespec *__restrict, const sigset_t *__restrict);
__SPFXD_END_DECLS
#endif
