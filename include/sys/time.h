/* lib-spfxd — <sys/time.h> */
#ifndef _SYS_TIME_H
#define _SYS_TIME_H
#include <features.h>

#define __SPFXD_NEED_time_t
#define __SPFXD_NEED_suseconds_t
#define __SPFXD_NEED_struct_timeval
#define __SPFXD_NEED_struct_timespec
#include <bits/typedefs.h>
#include <sys/select.h>

#define ITIMER_REAL    0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF    2

struct itimerval { struct timeval it_interval, it_value; };
struct timezone { int tz_minuteswest, tz_dsttime; };

__SPFXD_BEGIN_DECLS
int gettimeofday(struct timeval *__restrict, void *__restrict);
int getitimer(int, struct itimerval *);
int setitimer(int, const struct itimerval *__restrict, struct itimerval *__restrict);
int utimes(const char *, const struct timeval[2]);
#if defined(__SPFXD_BSD)
int settimeofday(const struct timeval *, const struct timezone *);
int futimes(int, const struct timeval[2]);
int lutimes(const char *, const struct timeval[2]);
int futimesat(int, const char *, const struct timeval[2]);
int adjtime(const struct timeval *, struct timeval *);
#define timerisset(t) ((t)->tv_sec || (t)->tv_usec)
#define timerclear(t) ((t)->tv_sec = (t)->tv_usec = 0)
#define timercmp(s, t, op) ((s)->tv_sec == (t)->tv_sec ? \
	(s)->tv_usec op (t)->tv_usec : (s)->tv_sec op (t)->tv_sec)
#define timeradd(s, t, a) (void)((a)->tv_sec = (s)->tv_sec + (t)->tv_sec, \
	((a)->tv_usec = (s)->tv_usec + (t)->tv_usec) >= 1000000 && \
	((a)->tv_usec -= 1000000, (a)->tv_sec++))
#define timersub(s, t, a) (void)((a)->tv_sec = (s)->tv_sec - (t)->tv_sec, \
	((a)->tv_usec = (s)->tv_usec - (t)->tv_usec) < 0 && \
	((a)->tv_usec += 1000000, (a)->tv_sec--))
#define TIMEVAL_TO_TIMESPEC(tv, ts) ((ts)->tv_sec = (tv)->tv_sec, (ts)->tv_nsec = (tv)->tv_usec * 1000, (void)0)
#define TIMESPEC_TO_TIMEVAL(tv, ts) ((tv)->tv_sec = (ts)->tv_sec, (tv)->tv_usec = (ts)->tv_nsec / 1000, (void)0)
#endif
__SPFXD_END_DECLS
#endif
