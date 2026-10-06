/* lib-spfxd — <utime.h> */
#ifndef _UTIME_H
#define _UTIME_H
#include <features.h>
#define __SPFXD_NEED_time_t
#include <bits/typedefs.h>
struct utimbuf { time_t actime, modtime; };
__SPFXD_BEGIN_DECLS
int utime(const char *, const struct utimbuf *);
__SPFXD_END_DECLS
#endif
