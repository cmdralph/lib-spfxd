/*
 * lib-spfxd oracle test — localtime/mktime/strftime across time zones
 * (zoneinfo and POSIX TZ strings) over a wide range of instants.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void)
{
	static const char *zones[] = {
		"UTC0", "EST5EDT,M3.2.0,M11.1.0", "CET-1CEST,M3.5.0,M10.5.0/3", "<-03>3", "NZST-12NZDT,M9.5.0,M4.1.0/3",
		"IST-5:30", "America/New_York", "Europe/London", "Europe/Berlin", "Asia/Kolkata", "Australia/Sydney",
		"Australia/Lord_Howe", "America/Sao_Paulo", "Pacific/Chatham", "Asia/Tehran", "Africa/Casablanca",
		"America/St_Johns", "Pacific/Apia", "Europe/Dublin", "Asia/Kathmandu",
	};
	const char *fmt = "%Y-%m-%d %H:%M:%S %Z %z %a %j %U %W %V %G %u %w %p %I %e %C %y %s";
	for (size_t z = 0; z < sizeof zones / sizeof *zones; z++) {
		setenv("TZ", zones[z], 1);
		tzset();
		printf("== %s\n", zones[z]);
		/* glibc evaluates POSIX DST rules for years before 1970 with the
		 * 1970 transition dates; lib-spfxd applies them proleptically as
		 * POSIX specifies, so rule strings are compared from 1970 on */
		long long t0 = strchr(zones[z], ',') ? 0 : -2208988800LL;
		for (long long t = t0; t < 4102444800LL; t += 7777777) {
			time_t tt = (time_t)t;
			struct tm tm;
			if (!localtime_r(&tt, &tm)) { printf("%lld: fail\n", t); continue; }
			char b[200];
			strftime(b, sizeof b, fmt, &tm);
			struct tm copy = tm;
			time_t back = mktime(&copy);
			printf("%lld %s dst=%d off=%ld back=%lld\n", t, b, tm.tm_isdst, tm.tm_gmtoff, (long long)back);
		}
		/* local times around transitions, through mktime with tm_isdst=-1 */
		for (int mon = 0; mon < 12; mon++) {
			struct tm tm;
			memset(&tm, 0, sizeof tm);
			tm.tm_year = 123;
			tm.tm_mon = mon;
			tm.tm_mday = 15;
			tm.tm_hour = 2;
			tm.tm_min = 30;
			tm.tm_isdst = -1;
			time_t r = mktime(&tm);
			printf("mktime %d -> %lld %02d:%02d dst=%d\n", mon, (long long)r, tm.tm_hour, tm.tm_min, tm.tm_isdst);
		}
	}
	return 0;
}
