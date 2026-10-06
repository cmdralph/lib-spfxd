/*
 * lib-spfxd — strtol family.
 *
 * One parser handles every integer width.  Digits are decoded through a
 * table; overflow is detected before it happens with a precomputed
 * quotient/remainder of the limit, so the hot loop is a table lookup, a
 * compare and a multiply-add.  ISO C semantics: leading white space,
 * optional sign, "0x"/"0X" for base 16 or 0 (only when a hex digit
 * follows), leading "0" selects octal for base 0; on overflow the limit is
 * returned with ERANGE; with no digits *end = s and 0 is returned.  A
 * minus sign negates unsigned results (in the unsigned type).
 */
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include "libc.h"

static const unsigned char digval[256] = {
#define X 255
	X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,
	X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, 0,1,2,3,4,5,6,7,8,9,X,X,X,X,X,X,
	X,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
	25,26,27,28,29,30,31,32,33,34,35,X,X,X,X,X,
	X,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
	25,26,27,28,29,30,31,32,33,34,35,X,X,X,X,X,
	X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,
	X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,
	X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,
	X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X, X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,X,
#undef X
};

hidden const unsigned char *__digit_values(void) { return digval; }

/*
 * Parse with magnitude limit `lim` for a non-negative result and `nlim`
 * for the magnitude of a negative one.  Returns the magnitude; *neg and
 * *ovf report the sign and overflow.
 */
hidden uintmax_t __strto_core(const char *restrict s0, char **restrict end, int base,
	uintmax_t lim, uintmax_t nlim, int *negp, int *ovfp)
{
	const unsigned char *s = (const unsigned char *)s0;
	int neg = 0, ovf = 0;
	uintmax_t v = 0;

	if (base < 0 || base == 1 || base > 36) {
		errno = EINVAL;
		if (end) *end = (char *)s0;
		*negp = *ovfp = 0;
		return 0;
	}
	while (isspace(*s)) s++;
	if (*s == '+' || *s == '-') neg = *s++ == '-';

	if ((base == 0 || base == 16) && s[0] == '0' && (s[1] | 32) == 'x' && digval[s[2]] < 16) {
		s += 2;
		base = 16;
	} else if (base == 0) {
		base = s[0] == '0' ? 8 : 10;
	}

	const unsigned char *digits = s;
	uintmax_t max = neg ? nlim : lim;
	uintmax_t q = max / (unsigned)base;
	unsigned r = (unsigned)(max % (unsigned)base);
	unsigned d;
	for (; (d = digval[*s]) < (unsigned)base; s++) {
		if (v > q || (v == q && d > r)) {
			ovf = 1;
			while (digval[*++s] < (unsigned)base);
			break;
		}
		v = v * (unsigned)base + d;
	}
	if (s == digits) {
		if (end) *end = (char *)s0;
		*negp = *ovfp = 0;
		return 0;
	}
	if (end) *end = (char *)s;
	*negp = neg;
	*ovfp = ovf;
	return ovf ? max : v;
}

long long strtoll(const char *restrict s, char **restrict end, int base)
{
	int neg, ovf;
	uintmax_t v = __strto_core(s, end, base, LLONG_MAX, (uintmax_t)LLONG_MAX + 1, &neg, &ovf);
	if (ovf) {
		errno = ERANGE;
		return neg ? LLONG_MIN : LLONG_MAX;
	}
	return neg ? (long long)(0 - v) : (long long)v;
}

unsigned long long strtoull(const char *restrict s, char **restrict end, int base)
{
	int neg, ovf;
	uintmax_t v = __strto_core(s, end, base, ULLONG_MAX, ULLONG_MAX, &neg, &ovf);
	if (ovf) {
		errno = ERANGE;
		return ULLONG_MAX;
	}
	return neg ? 0 - v : v;
}

long strtol(const char *restrict s, char **restrict end, int base)
{
	return (long)strtoll(s, end, base);
}

unsigned long strtoul(const char *restrict s, char **restrict end, int base)
{
	return (unsigned long)strtoull(s, end, base);
}

intmax_t strtoimax(const char *restrict s, char **restrict end, int base)
{
	return strtoll(s, end, base);
}

uintmax_t strtoumax(const char *restrict s, char **restrict end, int base)
{
	return strtoull(s, end, base);
}

int atoi(const char *s)
{
	return (int)strtol(s, 0, 10);
}

long atol(const char *s)
{
	return strtol(s, 0, 10);
}

long long atoll(const char *s)
{
	return strtoll(s, 0, 10);
}
