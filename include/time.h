/* lib-spfxd — <time.h> */
#ifndef _TIME_H
#define _TIME_H
#include <features.h>

#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_time_t
#define __SPFXD_NEED_clock_t
#define __SPFXD_NEED_struct_timespec
#if defined(__SPFXD_POSIX)
#define __SPFXD_NEED_clockid_t
#define __SPFXD_NEED_timer_t
#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_locale_t
#define __SPFXD_NEED_struct_itimerspec
#endif
#include <bits/typedefs.h>

#ifdef __cplusplus
# define NULL 0L
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif

#define CLOCKS_PER_SEC 1000000L
#define TIME_UTC 1

struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
	long tm_gmtoff;
	const char *tm_zone;
};

__SPFXD_BEGIN_DECLS
clock_t clock(void);
time_t time(time_t *);
double difftime(time_t, time_t) __spfxd_const;
time_t mktime(struct tm *);
size_t strftime(char *__restrict, size_t, const char *__restrict, const struct tm *__restrict);
struct tm *gmtime(const time_t *);
struct tm *localtime(const time_t *);
char *asctime(const struct tm *);
char *ctime(const time_t *);
#if defined(__SPFXD_C11)
int timespec_get(struct timespec *, int);
int timespec_getres(struct timespec *, int);
#endif

#if defined(__SPFXD_POSIX)
#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3
#define CLOCK_MONOTONIC_RAW      4
#define CLOCK_REALTIME_COARSE    5
#define CLOCK_MONOTONIC_COARSE   6
#define CLOCK_BOOTTIME           7
#define CLOCK_REALTIME_ALARM     8
#define CLOCK_BOOTTIME_ALARM     9
#define CLOCK_TAI               11
#define TIMER_ABSTIME 1

struct sigevent;

size_t strftime_l(char *__restrict, size_t, const char *__restrict, const struct tm *__restrict, locale_t);
struct tm *gmtime_r(const time_t *__restrict, struct tm *__restrict);
struct tm *localtime_r(const time_t *__restrict, struct tm *__restrict);
char *asctime_r(const struct tm *__restrict, char *__restrict);
char *ctime_r(const time_t *, char *);
void tzset(void);
int nanosleep(const struct timespec *, struct timespec *);
int clock_getres(clockid_t, struct timespec *);
int clock_gettime(clockid_t, struct timespec *);
int clock_settime(clockid_t, const struct timespec *);
int clock_nanosleep(clockid_t, int, const struct timespec *, struct timespec *);
int clock_getcpuclockid(pid_t, clockid_t *);
int timer_create(clockid_t, struct sigevent *__restrict, timer_t *__restrict);
int timer_delete(timer_t);
int timer_settime(timer_t, int, const struct itimerspec *__restrict, struct itimerspec *__restrict);
int timer_gettime(timer_t, struct itimerspec *);
int timer_getoverrun(timer_t);
extern char *tzname[2];
#endif
#if defined(__SPFXD_XSI)
extern int daylight;
extern long timezone;
extern int getdate_err;
struct tm *getdate(const char *);
char *strptime(const char *__restrict, const char *__restrict, struct tm *__restrict);
#endif
#if defined(__SPFXD_BSD)
time_t timegm(struct tm *);
time_t timelocal(struct tm *);
int stime(const time_t *);
#endif
__SPFXD_END_DECLS
#endif
