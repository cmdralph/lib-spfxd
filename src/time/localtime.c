/* lib-spfxd — gmtime / localtime / mktime / timegm / asctime / ctime. */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "time_impl.h"

struct tm *gmtime_r(const time_t *restrict t, struct tm *restrict tm)
{
	if (__secs_to_tm(*t, tm)) {
		errno = EOVERFLOW;
		return 0;
	}
	tm->tm_isdst = 0;
	tm->tm_gmtoff = 0;
	tm->tm_zone = __tz_utc_name;
	return tm;
}

struct tm *gmtime(const time_t *t)
{
	static struct tm tm;
	return gmtime_r(t, &tm);
}

struct tm *localtime_r(const time_t *restrict t, struct tm *restrict tm)
{
	long off, corr;
	int dst, leap;
	const char *abbr;
	/* keep the local-time computation from overflowing */
	if (*t < INT64_MIN / 2 || *t > INT64_MAX / 2) {
		errno = EOVERFLOW;
		return 0;
	}
	__tz_local(*t, &off, &dst, &abbr, &corr, &leap);
	if (__secs_to_tm(*t + off - corr, tm)) {
		errno = EOVERFLOW;
		return 0;
	}
	if (leap) tm->tm_sec = 60;
	tm->tm_isdst = dst;
	tm->tm_gmtoff = off;
	tm->tm_zone = abbr;
	return tm;
}

struct tm *localtime(const time_t *t)
{
	static struct tm tm;
	return localtime_r(t, &tm);
}

time_t mktime(struct tm *tm)
{
	struct tm n;
	int64_t local = __tm_to_secs(tm);
	int64_t t = __tz_mktime(tm, local);
	time_t tt = (time_t)t;
	if (!localtime_r(&tt, &n)) return -1;
	*tm = n;
	return tt;
}

time_t timelocal(struct tm *tm)
{
	return mktime(tm);
}

time_t timegm(struct tm *tm)
{
	struct tm n;
	time_t t = (time_t)__tm_to_secs(tm);
	if (__secs_to_tm(t, &n)) {
		errno = EOVERFLOW;
		return -1;
	}
	n.tm_isdst = 0;
	n.tm_gmtoff = 0;
	n.tm_zone = __tz_utc_name;
	*tm = n;
	return t;
}

char *asctime_r(const struct tm *restrict tm, char *restrict buf)
{
	static const char wd[7][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char mo[12][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	if ((unsigned)tm->tm_wday > 6 || (unsigned)tm->tm_mon > 11 ||
	    tm->tm_year > INT_MAX - 1900) {
		errno = EOVERFLOW;
		return 0;
	}
	/* buffers are 26 bytes by contract; 4-digit years fit exactly */
	if (tm->tm_year + 1900 > 9999 || tm->tm_year + 1900 < -999) {
		errno = EOVERFLOW;
		return 0;
	}
	snprintf(buf, 26, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n", wd[tm->tm_wday], mo[tm->tm_mon],
		tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900);
	return buf;
}

char *asctime(const struct tm *tm)
{
	static char buf[26];
	return asctime_r(tm, buf);
}

char *ctime_r(const time_t *t, char *buf)
{
	struct tm tm;
	return localtime_r(t, &tm) ? asctime_r(&tm, buf) : 0;
}

char *ctime(const time_t *t)
{
	struct tm *tm = localtime(t);
	return tm ? asctime(tm) : 0;
}
