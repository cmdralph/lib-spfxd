/*
 * lib-spfxd — proleptic Gregorian calendar arithmetic.
 *
 * Dates are converted with the 400-year-era method: shifting the year to
 * start in March puts the leap day at the end, after which the day of the
 * year follows from a linear formula on the month, and eras of 146097
 * days repeat exactly.  All arithmetic is 64-bit, so every time_t whose
 * year fits in an int converts exactly, including negative timestamps.
 */
#include <errno.h>
#include <limits.h>
#include "time_impl.h"

hidden const char __weekday_names[7][10] = {
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
hidden const char __month_names[12][10] = {
	"January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December"
};

static int64_t floordiv(int64_t a, int64_t b)
{
	int64_t q = a / b;
	return q - ((a % b != 0) & ((a < 0) != (b < 0)));
}

hidden int64_t __days_from_civil(int64_t y, int m, int d)
{
	/* m in 1..12 */
	y -= m <= 2;
	int64_t era = floordiv(y, 400);
	int64_t yoe = y - era * 400;                                /* [0, 399] */
	int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; /* [0, 365] */
	int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;        /* [0, 146096] */
	return era * 146097 + doe - 719468;
}

static int is_leap(int64_t y)
{
	return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

hidden int __civil_from_days(int64_t z, struct tm *tm)
{
	int64_t days = z;
	z += 719468;
	int64_t era = floordiv(z, 146097);
	int64_t doe = z - era * 146097;                                 /* [0, 146096] */
	int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; /* [0, 399] */
	int64_t y = yoe + era * 400;
	int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);          /* [0, 365] */
	int64_t mp = (5 * doy + 2) / 153;                               /* [0, 11] */
	int d = (int)(doy - (153 * mp + 2) / 5 + 1);
	int m = (int)(mp < 10 ? mp + 3 : mp - 9);
	y += m <= 2;
	if (y - 1900 > INT_MAX || y - 1900 < INT_MIN) return -1;
	tm->tm_year = (int)(y - 1900);
	tm->tm_mon = m - 1;
	tm->tm_mday = d;
	tm->tm_wday = (int)((days % 7 + 11) % 7);       /* 1970-01-01 was a Thursday */
	static const short cum[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	tm->tm_yday = cum[m - 1] + d - 1 + (m > 2 && is_leap(y));
	return 0;
}

hidden int64_t __tm_to_secs(const struct tm *tm)
{
	int64_t year = (int64_t)tm->tm_year + 1900;
	int64_t mon = tm->tm_mon;
	year += floordiv(mon, 12);
	mon -= floordiv(mon, 12) * 12;
	int64_t days = __days_from_civil(year, (int)mon + 1, 1) + tm->tm_mday - 1;
	return days * 86400 + (int64_t)tm->tm_hour * 3600 + (int64_t)tm->tm_min * 60 + tm->tm_sec;
}

hidden int __secs_to_tm(int64_t t, struct tm *tm)
{
	int64_t days = floordiv(t, 86400);
	int64_t rem = t - days * 86400;
	if (__civil_from_days(days, tm)) return -1;
	tm->tm_hour = (int)(rem / 3600);
	tm->tm_min = (int)(rem / 60 % 60);
	tm->tm_sec = (int)(rem % 60);
	return 0;
}
