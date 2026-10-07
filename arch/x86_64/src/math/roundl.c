/*
 * lib-spfxd — long double rounding to integral values (x87 extended).
 *
 * The significand has an explicit integer bit, so for unbiased exponent e
 * in [0, 62] the fraction bits are the low 63 - e bits of the significand.
 * floorl/ceill/truncl/roundl never raise inexact; rintl uses frndint, which
 * rounds in the current x87 rounding mode.
 */
#include <errno.h>
#include <fenv.h>
#include <limits.h>
#include <math.h>
#include "fp.h"

long double floorl(long double x)
{
	union ldshape u = { x };
	int e = (u.i.se & 0x7fff) - 0x3fff;
	int neg = u.i.se >> 15;
	if (e >= 63) return e == 0x4000 ? x + x : x;
	if (e < 0) {
		if (!u.i.m) return x;
		return neg ? -1.0L : 0.0L;
	}
	uint64_t m = (~0ULL) >> (e + 1);
	if (!(u.i.m & m)) return x;
	u.i.m &= ~m;
	if (neg) return u.f - 1.0L;     /* exact: integral and |u.f| < 2^63 */
	return u.f;
}

long double ceill(long double x)
{
	union ldshape u = { x };
	int e = (u.i.se & 0x7fff) - 0x3fff;
	int neg = u.i.se >> 15;
	if (e >= 63) return e == 0x4000 ? x + x : x;
	if (e < 0) {
		if (!u.i.m) return x;
		return neg ? -0.0L : 1.0L;
	}
	uint64_t m = (~0ULL) >> (e + 1);
	if (!(u.i.m & m)) return x;
	u.i.m &= ~m;
	if (!neg) return u.f + 1.0L;
	return u.f;
}

long double truncl(long double x)
{
	union ldshape u = { x };
	int e = (u.i.se & 0x7fff) - 0x3fff;
	if (e >= 63) return e == 0x4000 ? x + x : x;
	if (e < 0) {
		u.i.m = 0;
		u.i.se &= 0x8000;
		return u.f;
	}
	u.i.m &= ~((~0ULL) >> (e + 1));
	return u.f;
}

long double roundl(long double x)
{
	union ldshape u = { x };
	int e = (u.i.se & 0x7fff) - 0x3fff;
	int neg = u.i.se >> 15;
	if (e >= 63) return e == 0x4000 ? x + x : x;
	if (e < 0) {
		if (e == -1) return neg ? -1.0L : 1.0L;
		u.i.m = 0;
		u.i.se &= 0x8000;
		return u.f;
	}
	uint64_t m = (~0ULL) >> (e + 1);
	uint64_t f = u.i.m & m;
	if (!f) return x;
	int up = (f >> (62 - e)) & 1;   /* the 1/2 bit */
	u.i.m &= ~m;
	if (up) return neg ? u.f - 1.0L : u.f + 1.0L;
	return u.f;
}

long double roundevenl(long double x)
{
	long double r = roundl(x);
	if (fabsl(r - x) == 0.5L) r = 2.0L * roundl(x * 0.5L);
	return r;
}

long double rintl(long double x)
{
	long double y;
	__asm__ ("frndint" : "=t"(y) : "0"(x));
	return y;
}

long double nearbyintl(long double x)
{
	int saved = fetestexcept(FE_INEXACT);
	long double y = rintl(x);
	if (!saved) feclearexcept(FE_INEXACT);
	return y;
}

long lroundl(long double x)
{
	long double r = roundl(x);
	if (!(r >= -0x1p63L && r < 0x1p63L)) {
		feraiseexcept(FE_INVALID);
		errno = EDOM;
		return __builtin_signbit(x) ? LONG_MIN : LONG_MAX;
	}
	return (long)r;
}

long long llroundl(long double x)
{
	return lroundl(x);
}
