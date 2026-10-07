/*
 * lib-spfxd — strfmon / strfmon_l.
 *
 * lib-spfxd's locales define no monetary formatting (all LC_MONETARY data
 * is that of the POSIX locale), so values are formatted with no currency
 * symbol, '.' as the radix character, no grouping and two fractional
 * digits by default — the behaviour of other implementations in the C
 * locale.  All directive syntax is supported:
 *     %[=f ^ + ( ! -][width][#left][.right]{i,n,%}
 */
#include <errno.h>
#include <math.h>
#include <monetary.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static ssize_t vstrfmon(char *restrict s, size_t n, const char *restrict fmt, va_list ap)
{
	size_t o = 0;
	for (const char *p = fmt; *p; p++) {
		if (*p != '%') {
			if (o + 1 >= n) goto big;
			s[o++] = *p;
			continue;
		}
		p++;
		if (*p == '%') {
			if (o + 1 >= n) goto big;
			s[o++] = '%';
			continue;
		}
		char fill = ' ';
		int paren = 0, left = 0;
		for (;; p++) {
			if (*p == '=' && p[1]) { fill = *++p; continue; }
			if (*p == '^' || *p == '!' || *p == '+') continue;
			if (*p == '(') { paren = 1; continue; }
			if (*p == '-') { left = 1; continue; }
			break;
		}
		int width = 0, lp = -1, rp = 2;
		while (*p >= '0' && *p <= '9') width = width * 10 + (*p++ - '0');
		if (*p == '#') {
			lp = 0;
			for (p++; *p >= '0' && *p <= '9'; p++) lp = lp * 10 + (*p - '0');
		}
		if (*p == '.') {
			rp = 0;
			for (p++; *p >= '0' && *p <= '9'; p++) rp = rp * 10 + (*p - '0');
		}
		if (*p != 'i' && *p != 'n') {
			errno = EINVAL;
			return -1;
		}
		double v = va_arg(ap, double);
		int neg = signbit(v);
		char num[512], field[600];
		snprintf(num, sizeof num, "%.*f", rp > 100 ? 100 : rp, fabs(v));
		char *dot = strchr(num, '.');
		size_t ilen = dot ? (size_t)(dot - num) : strlen(num);
		size_t f = 0;
		if (paren && neg) field[f++] = '(';
		else if (lp >= 0) field[f++] = neg ? '-' : ' ';
		else if (neg) field[f++] = '-';
		for (int k = (int)ilen; k < lp && f < 300; k++) field[f++] = fill;
		size_t nl = strlen(num);
		if (f + nl + 2 >= sizeof field) nl = sizeof field - f - 2;
		memcpy(field + f, num, nl);
		f += nl;
		if (paren && neg) field[f++] = ')';
		size_t pad = (size_t)width > f ? (size_t)width - f : 0;
		if (o + f + pad + 1 > n) goto big;
		if (!left) { memset(s + o, ' ', pad); o += pad; }
		memcpy(s + o, field, f);
		o += f;
		if (left) { memset(s + o, ' ', pad); o += pad; }
	}
	if (o >= n) goto big;
	s[o] = 0;
	return (ssize_t)o;
big:
	errno = E2BIG;
	return -1;
}

ssize_t strfmon(char *restrict s, size_t n, const char *restrict fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ssize_t r = vstrfmon(s, n, fmt, ap);
	va_end(ap);
	return r;
}

ssize_t strfmon_l(char *restrict s, size_t n, locale_t l, const char *restrict fmt, ...)
{
	(void)l;
	va_list ap;
	va_start(ap, fmt);
	ssize_t r = vstrfmon(s, n, fmt, ap);
	va_end(ap);
	return r;
}
