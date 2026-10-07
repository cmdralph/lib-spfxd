/* lib-spfxd — <sys/timerfd.h> */
#ifndef _SYS_TIMERFD_H
#define _SYS_TIMERFD_H
#include <features.h>
#include <time.h>
#define __SPFXD_NEED_struct_timespec
#define __SPFXD_NEED_struct_itimerspec
#include <bits/typedefs.h>
#define TFD_NONBLOCK 04000
#define TFD_CLOEXEC 02000000
#define TFD_TIMER_ABSTIME 1
#define TFD_TIMER_CANCEL_ON_SET 2
__SPFXD_BEGIN_DECLS
int timerfd_create(int, int);
int timerfd_settime(int, int, const struct itimerspec *, struct itimerspec *);
int timerfd_gettime(int, struct itimerspec *);
__SPFXD_END_DECLS
#endif
