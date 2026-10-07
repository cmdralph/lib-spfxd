/*
 * lib-spfxd — strftime / strftime_l / wcsftime.
 *
 * All POSIX conversions plus the common GNU ones (%k %l %P %s %+), the
 * E and O modifiers (accepted, no alternative representations in the C
 * locale), and the GNU padding flags: '_' (spaces), '-' (no padding),
 * '0' (zeros), '^' (upper case), '#' (swap case) and a field width.
 */
#include <limits.h>
#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include "time_impl.h"

struct out {
	char *s;
	size_t n, len;
	int overflow;
};

static void put(struct out *o, const char *s, size_t l)
{
	if (o->overflow) return;
	if (o->len + l >= o->n) {
		o->overflow = 1;
		return;
	}
	memcpy(o->s + o->len, s, l);
	o->len += l;
}

static void put_padded(struct out *o, const char *s, size_t l, int width, char pad)
{
	for (long k = (long)width - (long)l; k > 0; k--) put(o, &pad, 1);
	put(o, s, l);
}

static int is_leap(long y)
{
	return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

/* ISO 8601 weeks start on Monday; week 1 contains the year's first
 * Thursday, so a year has 53 weeks iff Jan 1 is a Thursday, or a Wednesday
 * in a leap year. */
static int iso_weeks(long y, int jan1_wday)
{
	return (jan1_wday == 4 || (is_leap(y) && jan1_wday == 3)) ? 53 : 52;
}

static void iso_week(const struct tm *tm, long *year, int *week)
{
	long y = (long)tm->tm_year + 1900;
	int jan1 = ((tm->tm_wday - tm->tm_yday % 7) % 7 + 7) % 7;   /* Sunday = 0 */
	int wday = (tm->tm_wday + 6) % 7;                             /* Monday = 0 */
	int w = (tm->tm_yday - wday + 10) / 7;
	if (w < 1) {
		y--;
		int pjan1 = ((jan1 - (365 + is_leap(y)) % 7) % 7 + 7) % 7;
		w = iso_weeks(y, pjan1);
	} else if (w > iso_weeks(y, jan1)) {
		w = 1;
		y++;
	}
	*year = y;
	*week = w;
}

static size_t fmt_core(struct out *o, const char *f, const struct tm *tm);

static void case_xform(char *s, size_t l, int upper, int swap)
{
	for (size_t i = 0; i < l; i++) {
		unsigned char c = (unsigned char)s[i];
		if (upper) {
			if (c - 'a' < 26u) s[i] = (char)(c & 0x5f);
		} else if (swap) {
			if (c - 'a' < 26u) s[i] = (char)(c & 0x5f);
			else if (c - 'A' < 26u) s[i] = (char)(c | 0x20);
		}
	}
}

static size_t fmt_core(struct out *o, const char *f, const struct tm *tm)
{
	char buf[64];
	for (; *f; f++) {
		if (*f != '%') {
			put(o, f, 1);
			continue;
		}
		f++;
		char pad = 0;
		int upper = 0, swap = 0;
		for (;; f++) {
			if (*f == '_') pad = ' ';
			else if (*f == '-') pad = '-';
			else if (*f == '0') pad = '0';
			else if (*f == '^') upper = 1;
			else if (*f == '#') swap = 1;
			else break;
		}
		int width = 0;
		while (*f - '0' < 10u) width = width * 10 + (*f++ - '0');
		if (*f == 'E' || *f == 'O') f++;

		long num = 0;
		int ndig = 0;          /* >0: numeric conversion with this default width */
		char npad = '0';
		const char *str = 0;
		size_t sl = 0;
		const char *sub = 0;   /* composite conversions */

		switch (*f) {
		case 'a':
			str = (unsigned)tm->tm_wday < 7 ? __weekday_names[tm->tm_wday] : "?";
			sl = str[0] != '?' ? 3 : 1;
			break;
		case 'A':
			str = (unsigned)tm->tm_wday < 7 ? __weekday_names[tm->tm_wday] : "?";
			sl = strlen(str);
			break;
		case 'b': case 'h':
			str = (unsigned)tm->tm_mon < 12 ? __month_names[tm->tm_mon] : "?";
			sl = str[0] != '?' ? 3 : 1;
			break;
		case 'B':
			str = (unsigned)tm->tm_mon < 12 ? __month_names[tm->tm_mon] : "?";
			sl = strlen(str);
			break;
		case 'c': sub = "%a %b %e %H:%M:%S %Y"; break;
		case 'C': {
			long y = (long)tm->tm_year + 1900;
			num = y >= 0 ? y / 100 : -((-y + 99) / 100);
			ndig = 2;
			break;
		}
		case 'd': num = tm->tm_mday; ndig = 2; break;
		case 'D': sub = "%m/%d/%y"; break;
		case 'e': num = tm->tm_mday; ndig = 2; npad = ' '; break;
		case 'F': sub = "%Y-%m-%d"; break;
		case 'G': case 'g': case 'V': {
			long y;
			int w;
			iso_week(tm, &y, &w);
			if (*f == 'V') { num = w; ndig = 2; }
			else if (*f == 'g') { num = ((y % 100) + 100) % 100; ndig = 2; }
			else { num = y; ndig = 1; }
			break;
		}
		case 'H': num = tm->tm_hour; ndig = 2; break;
		case 'I': num = tm->tm_hour % 12 ? tm->tm_hour % 12 : 12; ndig = 2; break;
		case 'j': num = tm->tm_yday + 1; ndig = 3; break;
		case 'k': num = tm->tm_hour; ndig = 2; npad = ' '; break;
		case 'l': num = tm->tm_hour % 12 ? tm->tm_hour % 12 : 12; ndig = 2; npad = ' '; break;
		case 'm': num = tm->tm_mon + 1; ndig = 2; break;
		case 'M': num = tm->tm_min; ndig = 2; break;
		case 'n': str = "\n"; sl = 1; break;
		case 't': str = "\t"; sl = 1; break;
		case 'p':
			str = tm->tm_hour < 12 ? "AM" : "PM";
			sl = 2;
			if (swap) { swap = 0; str = tm->tm_hour < 12 ? "am" : "pm"; }
			break;
		case 'P': str = tm->tm_hour < 12 ? "am" : "pm"; sl = 2; break;
		case 'r': sub = "%I:%M:%S %p"; break;
		case 'R': sub = "%H:%M"; break;
		case 's': {
			struct tm copy = *tm;
			num = (long)mktime(&copy);
			ndig = 1;
			break;
		}
		case 'S': num = tm->tm_sec; ndig = 2; break;
		case 'T': sub = "%H:%M:%S"; break;
		case 'u': num = tm->tm_wday ? tm->tm_wday : 7; ndig = 1; break;
		case 'U': num = (tm->tm_yday + 7 - tm->tm_wday) / 7; ndig = 2; break;
		case 'w': num = tm->tm_wday; ndig = 1; break;
		case 'W': num = (tm->tm_yday + 7 - (tm->tm_wday + 6) % 7) / 7; ndig = 2; break;
		case 'x': sub = "%m/%d/%y"; break;
		case 'X': sub = "%H:%M:%S"; break;
		case 'y': num = (((long)tm->tm_year + 1900) % 100 + 100) % 100; ndig = 2; break;
		case 'Y': num = (long)tm->tm_year + 1900; ndig = 1; break;
		case 'z': {
			long off = tm->tm_gmtoff;
			char sign = off < 0 ? '-' : '+';
			if (off < 0) off = -off;
			int l = 0;
			buf[l++] = sign;
			long hh = off / 3600, mm = off / 60 % 60;
			buf[l++] = (char)('0' + hh / 10 % 10);
			buf[l++] = (char)('0' + hh % 10);
			buf[l++] = (char)('0' + mm / 10);
			buf[l++] = (char)('0' + mm % 10);
			put_padded(o, buf, (size_t)l, width, pad == ' ' ? ' ' : '0');
			continue;
		}
		case 'Z':
			str = tm->tm_zone ? tm->tm_zone : "";
			sl = strlen(str);
			break;
		case '+': sub = "%a %b %e %H:%M:%S %Z %Y"; break;
		case '%': str = "%"; sl = 1; break;
		case 0:
			f--;
			str = "%";
			sl = 1;
			break;
		default:
			/* unknown conversion: copy it literally */
			buf[0] = '%';
			buf[1] = *f;
			put(o, buf, 2);
			continue;
		}

		if (sub) {
			struct out tmp = { buf, sizeof buf, 0, 0 };
			fmt_core(&tmp, sub, tm);
			case_xform(buf, tmp.len, upper, swap);
			put_padded(o, buf, tmp.len, width, pad && pad != '-' ? pad : ' ');
			continue;
		}
		if (str) {
			size_t l = sl < sizeof buf ? sl : sizeof buf;
			memcpy(buf, str, l);
			case_xform(buf, l, upper, swap);
			put_padded(o, buf, l, pad == '-' ? 0 : width, pad == '0' ? '0' : ' ');
			continue;
		}
		/* numeric */
		{
			char digits[32], *e = digits + sizeof digits, *p = e;
			int neg = num < 0;
			unsigned long u = neg ? 0UL - (unsigned long)num : (unsigned long)num;
			do *--p = (char)('0' + u % 10); while (u /= 10);
			int w = width ? width : ndig;
			char pc = npad;
			if (pad == ' ') pc = ' ';
			else if (pad == '0') pc = '0';
			if (pad == '-') w = 0;
			size_t l = (size_t)(e - p) + neg;
			if (pc == '0') {
				if (neg) put(o, "-", 1);
				for (long k = w - (long)l; k > 0; k--) put(o, "0", 1);
				put(o, p, (size_t)(e - p));
			} else {
				for (long k = w - (long)l; k > 0; k--) put(o, " ", 1);
				if (neg) put(o, "-", 1);
				put(o, p, (size_t)(e - p));
			}
		}
	}
	return o->len;
}

size_t strftime(char *restrict s, size_t n, const char *restrict f, const struct tm *restrict tm)
{
	struct out o = { s, n, 0, 0 };
	if (!n) return 0;
	fmt_core(&o, f, tm);
	if (o.overflow) return 0;
	s[o.len] = 0;
	return o.len;
}

size_t strftime_l(char *restrict s, size_t n, const char *restrict f, const struct tm *restrict tm, locale_t l)
{
	return strftime(s, n, f, tm);
}

size_t wcsftime(wchar_t *restrict ws, size_t n, const wchar_t *restrict wf, const struct tm *restrict tm)
{
	char fmt[512], out[1024];
	mbstate_t st;
	const wchar_t *src = wf;
	memset(&st, 0, sizeof st);
	size_t fl = wcsrtombs(fmt, &src, sizeof fmt, &st);
	if (fl == (size_t)-1 || src) return 0;
	size_t l = strftime(out, sizeof out, fmt, tm);
	if (!l && fmt[0]) return 0;
	const char *in = out;
	memset(&st, 0, sizeof st);
	size_t wl = mbsrtowcs(ws, &in, n, &st);
	if (wl == (size_t)-1 || wl >= n) return 0;
	return wl;
}
