/* lib-spfxd — <sys/times.h> */
#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H
#include <features.h>
#define __SPFXD_NEED_clock_t
#include <bits/typedefs.h>
struct tms { clock_t tms_utime, tms_stime, tms_cutime, tms_cstime; };
__SPFXD_BEGIN_DECLS
clock_t times(struct tms *);
__SPFXD_END_DECLS
#endif
