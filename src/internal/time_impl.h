/* lib-spfxd — time conversion internals. */
#ifndef _SPFXD_TIME_IMPL_H
#define _SPFXD_TIME_IMPL_H

#include <time.h>
#include <stdint.h>
#include "libc.h"

/* Days since 1970-01-01 for a proleptic Gregorian date (any year). */
hidden int64_t __days_from_civil(int64_t y, int m, int d);
/* Inverse: fills tm_year/mon/mday/wday/yday; returns 0 or -1 on overflow. */
hidden int __civil_from_days(int64_t days, struct tm *tm);
/* Seconds since the epoch for broken-down UTC fields, normalizing them. */
hidden int64_t __tm_to_secs(const struct tm *tm);
/* Broken-down UTC time; -1 if the year does not fit in an int. */
hidden int __secs_to_tm(int64_t t, struct tm *tm);

/* Time zone queries (tz.c).  All lock internally. */
hidden void __tz_local(int64_t t, long *gmtoff, int *isdst, const char **abbr,
	long *leapcorr, int *leap_sec);
hidden int64_t __tz_mktime(struct tm *tm, int64_t local_secs);
hidden const char *__tz_utc_name;

extern hidden const char __weekday_names[7][10];
extern hidden const char __month_names[12][10];

#endif
