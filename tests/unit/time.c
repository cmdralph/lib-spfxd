/* lib-spfxd test — time: clocks, sleeping, broken-down time, mktime
 * normalization, timegm, strftime/strptime, time zones (TZ strings and
 * zoneinfo), far past and future, leap seconds. */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "t.h"

static long long ns_of(struct timespec t) { return t.tv_sec * 1000000000LL + t.tv_nsec; }

int main(void)
{
	struct timespec a, b, res;
	CHECK(!clock_gettime(CLOCK_MONOTONIC, &a) && !clock_getres(CLOCK_MONOTONIC, &res) && res.tv_nsec > 0, "monotonic");
	struct timespec req = { 0, 20000000 };
	CHECK(!nanosleep(&req, 0), "nanosleep");
	clock_gettime(CLOCK_MONOTONIC, &b);
	CHECK(ns_of(b) - ns_of(a) >= 20000000, "slept at least 20ms (%lld ns)", ns_of(b) - ns_of(a));
	CHECK(!clock_nanosleep(CLOCK_MONOTONIC, 0, &req, 0), "clock_nanosleep");
	clock_gettime(CLOCK_MONOTONIC, &a);
	a.tv_nsec += 5000000;
	if (a.tv_nsec >= 1000000000) { a.tv_sec++; a.tv_nsec -= 1000000000; }
	CHECK(!clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &a, 0), "absolute clock_nanosleep");
	time_t now = time(0);
	struct timeval tv;
	gettimeofday(&tv, 0);
	CHECK(now > 1700000000 && labs(tv.tv_sec - now) <= 1, "time/gettimeofday agree");
	CHECK(clock() >= 0 && CLOCKS_PER_SEC == 1000000, "clock");
	CHECK(timespec_get(&a, TIME_UTC) == TIME_UTC, "timespec_get");
	CHECK(difftime(10, 4) == 6.0, "difftime");

	/* UTC conversions incl. far past/future and negative times */
	setenv("TZ", "UTC0", 1);
	tzset();
	struct tm tm;
	time_t t = 0;
	CHECK(gmtime_r(&t, &tm) && tm.tm_year == 70 && tm.tm_yday == 0 && tm.tm_wday == 4, "epoch");
	t = -1;
	CHECK(gmtime_r(&t, &tm) && tm.tm_year == 69 && tm.tm_mon == 11 && tm.tm_mday == 31 && tm.tm_hour == 23, "before epoch");
	t = 951782400;  /* 2000-02-29 */
	CHECK(gmtime_r(&t, &tm) && tm.tm_mon == 1 && tm.tm_mday == 29 && tm.tm_yday == 59, "leap day 2000");
	t = 253402300799LL;  /* 9999-12-31T23:59:59 */
	CHECK(gmtime_r(&t, &tm) && tm.tm_year == 8099 && tm.tm_mon == 11 && tm.tm_sec == 59, "year 9999");
	t = -62135596800LL;  /* 0001-01-01 */
	CHECK(gmtime_r(&t, &tm) && tm.tm_year == -1899 && tm.tm_mon == 0 && tm.tm_mday == 1 && tm.tm_wday == 1, "year 1");
	t = 67768036191676799LL;  /* largest time with a 32-bit tm_year */
	CHECK(gmtime_r(&t, &tm) && tm.tm_year == INT_MAX, "max year");
	t++;
	errno = 0;
	CHECK(!gmtime_r(&t, &tm) && errno == EOVERFLOW, "gmtime overflow");
	/* round trip timegm over a wide range */
	int ok = 1;
	for (long long x = -100000000000LL; x < 100000000000LL; x += 98765432109LL / 7) {
		time_t tt = (time_t)x;
		gmtime_r(&tt, &tm);
		if (timegm(&tm) != tt) ok = 0;
	}
	CHECK(ok, "timegm(gmtime(t)) == t");
	/* mktime normalization */
	memset(&tm, 0, sizeof tm);
	tm.tm_year = 120; tm.tm_mon = 13; tm.tm_mday = 32; tm.tm_hour = -1; tm.tm_isdst = 0;
	t = mktime(&tm);
	CHECK(tm.tm_year == 121 && tm.tm_mon == 2 && tm.tm_mday == 3 && tm.tm_hour == 23, "mktime normalizes %d-%d-%d %d",
	      tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour);

	/* POSIX TZ strings with DST rules */
	setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
	tzset();
	t = 1688216400;  /* 2023-07-01 13:00 UTC */
	localtime_r(&t, &tm);
	CHECK(tm.tm_hour == 9 && tm.tm_isdst == 1 && tm.tm_gmtoff == -4 * 3600 && !strcmp(tm.tm_zone, "EDT"), "EDT");
	t = 1672578000;  /* 2023-01-01 13:00 UTC */
	localtime_r(&t, &tm);
	CHECK(tm.tm_hour == 8 && tm.tm_isdst == 0 && !strcmp(tm.tm_zone, "EST"), "EST");
	CHECK(timezone == 5 * 3600 && daylight == 1 && !strcmp(tzname[0], "EST") && !strcmp(tzname[1], "EDT"), "tz globals");
	/* spring-forward gap and fall-back overlap */
	memset(&tm, 0, sizeof tm);
	tm.tm_year = 123; tm.tm_mon = 2; tm.tm_mday = 12; tm.tm_hour = 2; tm.tm_min = 30; tm.tm_isdst = -1;
	t = mktime(&tm);
	CHECK(t != -1 && tm.tm_hour == 3 && tm.tm_isdst == 1, "nonexistent local time moved forward (%d)", tm.tm_hour);
	memset(&tm, 0, sizeof tm);
	tm.tm_year = 123; tm.tm_mon = 10; tm.tm_mday = 5; tm.tm_hour = 1; tm.tm_min = 30; tm.tm_isdst = 0;
	CHECK(mktime(&tm) == 1699165800, "ambiguous time with tm_isdst=0 picks standard");
	setenv("TZ", "<+0530>-5:30", 1);
	tzset();
	t = 0;
	localtime_r(&t, &tm);
	CHECK(tm.tm_hour == 5 && tm.tm_min == 30 && !strcmp(tm.tm_zone, "+0530"), "quoted TZ name with minutes");

	/* zoneinfo files */
	setenv("TZ", "America/New_York", 1);
	tzset();
	t = 1688216400;
	localtime_r(&t, &tm);
	if (tm.tm_gmtoff == 0) {
		SKIP("no zoneinfo database");
	} else {
		CHECK(tm.tm_hour == 9 && !strcmp(tm.tm_zone, "EDT"), "zoneinfo New York");
		t = -1000000000;  /* 1938-04-24: historical rules, already DST */
		localtime_r(&t, &tm);
		CHECK(tm.tm_gmtoff == -4 * 3600 && tm.tm_isdst, "1938 New York offset");
		t = -2000000000;  /* 1906 */
		localtime_r(&t, &tm);
		CHECK(tm.tm_gmtoff == -5 * 3600 && !tm.tm_isdst, "1906 New York offset");
		t = 4102444800LL;  /* 2100: beyond the transition table, from the footer rule */
		localtime_r(&t, &tm);
		CHECK(tm.tm_gmtoff == -5 * 3600 && !strcmp(tm.tm_zone, "EST"), "2100 from footer rule");
		setenv("TZ", "Australia/Lord_Howe", 1);
		tzset();
		t = 1672531200;  /* January: half-hour DST */
		localtime_r(&t, &tm);
		CHECK(tm.tm_gmtoff == 11 * 3600 && tm.tm_isdst, "Lord Howe half-hour DST");
		setenv("TZ", "right/UTC", 1);
		tzset();
		t = 1483228826;  /* 2016-12-31T23:59:60 in right/ (leap second) */
		localtime_r(&t, &tm);
		if (tm.tm_sec == 60) CHECK(tm.tm_min == 59 && tm.tm_hour == 23 && tm.tm_mday == 31, "leap second shown as :60");
		else SKIP("no right/ zoneinfo");
	}
	setenv("TZ", "UTC0", 1);
	tzset();

	/* strftime / strptime */
	t = 1234567890;
	gmtime_r(&t, &tm);
	char buf[128];
	CHECK(strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S %a %b %j %U %W %V %G %u %w %e %C %y %%", &tm) &&
	      !strcmp(buf, "2009-02-13 23:31:30 Fri Feb 044 06 06 07 2009 5 5 13 20 09 %"), "strftime '%s'", buf);
	CHECK(strftime(buf, sizeof buf, "%s %z %Z %D %T %R %F %r", &tm) &&
	      !strcmp(buf, "1234567890 +0000 UTC 02/13/09 23:31:30 23:31 2009-02-13 11:31:30 PM"), "strftime '%s'", buf);
	CHECK(strftime(buf, 5, "%Y-%m", &tm) == 0, "strftime overflow returns 0");
	CHECK(strftime(buf, sizeof buf, "%_5d|%-m|%010Y|%^a", &tm) && !strcmp(buf, "   13|2|0000002009|FRI"), "GNU flags '%s'", buf);
	memset(&tm, 0, sizeof tm);
	char *e = strptime("2024-02-29 13:45:07", "%Y-%m-%d %H:%M:%S", &tm);
	CHECK(e && !*e && tm.tm_year == 124 && tm.tm_mon == 1 && tm.tm_mday == 29 && tm.tm_sec == 7, "strptime");
	memset(&tm, 0, sizeof tm);
	e = strptime("Mon, 15 Jan 2024 08:00", "%a, %d %b %Y %H:%M", &tm);
	CHECK(e && !*e && tm.tm_mon == 0 && tm.tm_wday == 1, "strptime names");
	CHECK(!strptime("2024-13-01", "%Y-%m-%d", &tm), "strptime rejects month 13");
	t = 0;
	CHECK(!strcmp(asctime(gmtime(&t)), "Thu Jan  1 00:00:00 1970\n") && ctime(&t), "asctime");
	return DONE();
}
