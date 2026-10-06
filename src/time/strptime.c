/*
 * lib-spfxd — strptime (POSIX, plus %s %z %F %T %R %D %e %k %l %C %G/%g/%V
 * parsed-and-ignored) and getdate (XSI).
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "time_impl.h"

static const char *num(const char *s, int min, int max, int maxdig, int *out)
{
	int v = 0, n = 0, neg = 0;
	while (isspace((unsigned char)*s)) s++;
	if (min < 0 && (*s == '-' || *s == '+')) neg = *s++ == '-';
	for (; n < maxdig && *s - '0' < 10u; n++, s++) v = v * 10 + (*s - '0');
	if (!n) return 0;
	if (neg) v = -v;
	if (v < min || v > max) return 0;
	*out = v;
	return s;
}

static const char *name(const char *s, const char (*names)[10], int count, int *out)
{
	for (int i = 0; i < count; i++) {
		size_t full = strlen(names[i]);
		if (!strncasecmp(s, names[i], full)) {
			*out = i;
			return s + full;
		}
	}
	for (int i = 0; i < count; i++) {
		if (!strncasecmp(s, names[i], 3)) {
			*out = i;
			return s + 3;
		}
	}
	return 0;
}

char *strptime(const char *restrict s, const char *restrict f, struct tm *restrict tm)
{
	int v, century = -1, have_yy = 0, yy = 0, pm = -1;
	for (; *f; f++) {
		if (isspace((unsigned char)*f)) {
			while (isspace((unsigned char)*s)) s++;
			continue;
		}
		if (*f != '%') {
			if (*s++ != *f) return 0;
			continue;
		}
		f++;
		if (*f == 'E' || *f == 'O') f++;
		switch (*f) {
		case 'a': case 'A':
			if (!(s = name(s, __weekday_names, 7, &tm->tm_wday))) return 0;
			break;
		case 'b': case 'B': case 'h':
			if (!(s = name(s, __month_names, 12, &tm->tm_mon))) return 0;
			break;
		case 'c':
			if (!(s = strptime(s, "%a %b %e %H:%M:%S %Y", tm))) return 0;
			break;
		case 'C':
			if (!(s = num(s, 0, 99, 2, &century))) return 0;
			break;
		case 'd': case 'e':
			if (!(s = num(s, 1, 31, 2, &tm->tm_mday))) return 0;
			break;
		case 'D': case 'x':
			if (!(s = strptime(s, "%m/%d/%y", tm))) return 0;
			break;
		case 'F':
			if (!(s = strptime(s, "%Y-%m-%d", tm))) return 0;
			break;
		case 'H': case 'k':
			if (!(s = num(s, 0, 23, 2, &tm->tm_hour))) return 0;
			break;
		case 'I': case 'l':
			if (!(s = num(s, 1, 12, 2, &tm->tm_hour))) return 0;
			if (pm >= 0) tm->tm_hour = tm->tm_hour % 12 + 12 * pm;
			break;
		case 'j':
			if (!(s = num(s, 1, 366, 3, &v))) return 0;
			tm->tm_yday = v - 1;
			break;
		case 'm':
			if (!(s = num(s, 1, 12, 2, &v))) return 0;
			tm->tm_mon = v - 1;
			break;
		case 'M':
			if (!(s = num(s, 0, 59, 2, &tm->tm_min))) return 0;
			break;
		case 'n': case 't':
			while (isspace((unsigned char)*s)) s++;
			break;
		case 'p': case 'P':
			while (isspace((unsigned char)*s)) s++;
			if (!strncasecmp(s, "am", 2)) pm = 0;
			else if (!strncasecmp(s, "pm", 2)) pm = 1;
			else return 0;
			s += 2;
			if (tm->tm_hour <= 12) tm->tm_hour = tm->tm_hour % 12 + 12 * pm;
			break;
		case 'r':
			if (!(s = strptime(s, "%I:%M:%S %p", tm))) return 0;
			break;
		case 'R':
			if (!(s = strptime(s, "%H:%M", tm))) return 0;
			break;
		case 's': {
			char *end;
			errno = 0;
			long long t = strtoll(s, &end, 10);
			if (end == s || errno) return 0;
			time_t tt = (time_t)t;
			if (!localtime_r(&tt, tm)) return 0;
			s = end;
			break;
		}
		case 'S':
			if (!(s = num(s, 0, 61, 2, &tm->tm_sec))) return 0;
			break;
		case 'T': case 'X':
			if (!(s = strptime(s, "%H:%M:%S", tm))) return 0;
			break;
		case 'u':
			if (!(s = num(s, 1, 7, 1, &v))) return 0;
			tm->tm_wday = v % 7;
			break;
		case 'w':
			if (!(s = num(s, 0, 6, 1, &tm->tm_wday))) return 0;
			break;
		case 'U': case 'W': case 'V':
			if (!(s = num(s, 0, 53, 2, &v))) return 0;
			break;
		case 'g':
			if (!(s = num(s, 0, 99, 2, &v))) return 0;
			break;
		case 'G':
			if (!(s = num(s, INT_MIN + 1900, INT_MAX, 9, &v))) return 0;
			break;
		case 'y':
			if (!(s = num(s, 0, 99, 2, &yy))) return 0;
			have_yy = 1;
			break;
		case 'Y':
			if (!(s = num(s, INT_MIN + 1900, INT_MAX, 9, &v))) return 0;
			tm->tm_year = v - 1900;
			have_yy = 0;
			break;
		case 'z': {
			int sign, hh, mm = 0;
			while (isspace((unsigned char)*s)) s++;
			if (*s == 'Z') { s++; tm->tm_gmtoff = 0; break; }
			if (*s != '+' && *s != '-') return 0;
			sign = *s++ == '-' ? -1 : 1;
			if (!(s = num(s, 0, 99, 2, &hh))) return 0;
			if (*s == ':') s++;
			if (*s - '0' < 10u && !(s = num(s, 0, 59, 2, &mm))) return 0;
			tm->tm_gmtoff = sign * (hh * 3600L + mm * 60L);
			break;
		}
		case 'Z':
			while (isalpha((unsigned char)*s)) s++;
			break;
		case '%':
			while (isspace((unsigned char)*s)) s++;
			if (*s++ != '%') return 0;
			break;
		default:
			return 0;
		}
	}
	if (have_yy) {
		if (century >= 0) tm->tm_year = century * 100 + yy - 1900;
		else tm->tm_year = yy < 69 ? yy + 100 : yy;
	} else if (century >= 0) {
		tm->tm_year = century * 100 - 1900 + (tm->tm_year + 1900) % 100;
	}
	return (char *)s;
}

int getdate_err;

/* getdate: try each template from the file named by DATEMSK. */
struct tm *getdate(const char *s)
{
	static struct tm tm;
	const char *path = getenv("DATEMSK");
	if (!path || !*path) {
		getdate_err = 1;
		return 0;
	}
	FILE *f = fopen(path, "re");
	if (!f) {
		getdate_err = 2;
		return 0;
	}
	char line[256];
	struct tm *res = 0;
	getdate_err = 7;
	while (fgets(line, sizeof line, f)) {
		size_t l = strlen(line);
		if (l && line[l - 1] == '\n') line[--l] = 0;
		memset(&tm, 0, sizeof tm);
		tm.tm_mday = 1;
		tm.tm_isdst = -1;
		tm.tm_year = INT_MIN;
		const char *end = strptime(s, line, &tm);
		if (!end) continue;
		while (isspace((unsigned char)*end)) end++;
		if (*end) continue;
		if (tm.tm_year == INT_MIN) {
			time_t now = time(0);
			struct tm cur;
			localtime_r(&now, &cur);
			tm.tm_year = cur.tm_year;
		}
		if (mktime(&tm) == -1) {
			getdate_err = 8;
			continue;
		}
		res = &tm;
		break;
	}
	if (ferror(f)) getdate_err = 5;
	fclose(f);
	return res;
}
