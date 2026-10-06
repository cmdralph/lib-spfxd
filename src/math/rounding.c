/*
 * lib-spfxd — rounding to integral values: floor, ceil, trunc, round,
 * roundeven, rint, nearbyint, lround/llround (double, float, long double).
 *
 * floor/ceil/trunc/round work on the bit pattern and never raise inexact
 * (the C2x behaviour).  rint adds and subtracts 2^52 (2^23, 2^63), which
 * rounds in the current rounding mode and raises inexact exactly when the
 * result differs from x.  lrint/llrint live in arch/x86_64/src/math.
 */
#include <errno.h>
#include <fenv.h>
#include <limits.h>
#include <math.h>
#include "fp.h"

/* ---------------------------------------------------------------- double */

double floor(double x)
{
	uint64_t u = asuint64(x);
	int e = (int)(u >> 52 & 0x7ff) - 0x3ff;
	if (e >= 52) return e == 0x400 ? x + x : x;
	if (e < 0) {
		if (!(u << 1)) return x;
		return (u >> 63) ? -1.0 : 0.0;
	}
	uint64_t m = 0x000fffffffffffffULL >> e;
	if (!(u & m)) return x;
	if (u >> 63) u += m;
	return asdouble(u & ~m);
}

double ceil(double x)
{
	uint64_t u = asuint64(x);
	int e = (int)(u >> 52 & 0x7ff) - 0x3ff;
	if (e >= 52) return e == 0x400 ? x + x : x;
	if (e < 0) {
		if (!(u << 1)) return x;
		return (u >> 63) ? -0.0 : 1.0;
	}
	uint64_t m = 0x000fffffffffffffULL >> e;
	if (!(u & m)) return x;
	if (!(u >> 63)) u += m;
	return asdouble(u & ~m);
}

double trunc(double x)
{
	uint64_t u = asuint64(x);
	int e = (int)(u >> 52 & 0x7ff) - 0x3ff;
	if (e >= 52) return e == 0x400 ? x + x : x;
	if (e < 0) return asdouble(u & (1ULL << 63));
	return asdouble(u & ~(0x000fffffffffffffULL >> e));
}

double round(double x)
{
	uint64_t u = asuint64(x);
	int e = (int)(u >> 52 & 0x7ff) - 0x3ff;
	if (e >= 52) return e == 0x400 ? x + x : x;
	if (e < 0) {
		uint64_t s = u & (1ULL << 63);
		return asdouble(e == -1 ? s | 0x3ff0000000000000ULL : s);
	}
	uint64_t m = 0x000fffffffffffffULL >> e;
	if (!(u & m)) return x;
	u += 0x0008000000000000ULL >> e;
	return asdouble(u & ~m);
}

double roundeven(double x)
{
	uint64_t u = asuint64(x);
	int e = (int)(u >> 52 & 0x7ff) - 0x3ff;
	if (e >= 52) return e == 0x400 ? x + x : x;
	uint64_t s = u & (1ULL << 63);
	if (e < 0) {
		/* (0.5, 1) rounds to 1; 0.5 itself to 0 */
		if (e == -1 && (u & 0x000fffffffffffffULL)) return asdouble(s | 0x3ff0000000000000ULL);
		return asdouble(s);
	}
	uint64_t m = 0x000fffffffffffffULL >> e, half = 0x0008000000000000ULL >> e;
	uint64_t f = u & m;
	if (!f) return x;
	uint64_t one = m + 1;
	if (f > half || (f == half && (u & one))) u += one;
	return asdouble(u & ~m);
}

double rint(double x)
{
	uint64_t u = asuint64(x);
	int e = (int)(u >> 52 & 0x7ff);
	if (e >= 0x3ff + 52) return e == 0x7ff ? x + x : x;
	double y = u >> 63 ? (x - 0x1p52) + 0x1p52 : (x + 0x1p52) - 0x1p52;
	return y == 0 ? asdouble(u & (1ULL << 63)) : y;
}

double nearbyint(double x)
{
	int saved = fetestexcept(FE_INEXACT);
	double y = rint(x);
	if (!saved) feclearexcept(FE_INEXACT);
	return y;
}

long lround(double x)
{
	double r = round(x);
	if (!(r >= -0x1p63 && r < 0x1p63)) {
		feraiseexcept(FE_INVALID);
		errno = EDOM;
		return __builtin_signbit(x) ? LONG_MIN : LONG_MAX;
	}
	return (long)r;
}

long long llround(double x)
{
	return lround(x);
}

/* ---------------------------------------------------------------- float */

float floorf(float x)
{
	uint32_t u = asuint(x);
	int e = (int)(u >> 23 & 0xff) - 0x7f;
	if (e >= 23) return e == 0x80 ? x + x : x;
	if (e < 0) {
		if (!(u << 1)) return x;
		return (u >> 31) ? -1.0f : 0.0f;
	}
	uint32_t m = 0x007fffffu >> e;
	if (!(u & m)) return x;
	if (u >> 31) u += m;
	return asfloat(u & ~m);
}

float ceilf(float x)
{
	uint32_t u = asuint(x);
	int e = (int)(u >> 23 & 0xff) - 0x7f;
	if (e >= 23) return e == 0x80 ? x + x : x;
	if (e < 0) {
		if (!(u << 1)) return x;
		return (u >> 31) ? -0.0f : 1.0f;
	}
	uint32_t m = 0x007fffffu >> e;
	if (!(u & m)) return x;
	if (!(u >> 31)) u += m;
	return asfloat(u & ~m);
}

float truncf(float x)
{
	uint32_t u = asuint(x);
	int e = (int)(u >> 23 & 0xff) - 0x7f;
	if (e >= 23) return e == 0x80 ? x + x : x;
	if (e < 0) return asfloat(u & 0x80000000u);
	return asfloat(u & ~(0x007fffffu >> e));
}

float roundf(float x)
{
	uint32_t u = asuint(x);
	int e = (int)(u >> 23 & 0xff) - 0x7f;
	if (e >= 23) return e == 0x80 ? x + x : x;
	if (e < 0) {
		uint32_t s = u & 0x80000000u;
		return asfloat(e == -1 ? s | 0x3f800000u : s);
	}
	uint32_t m = 0x007fffffu >> e;
	if (!(u & m)) return x;
	u += 0x00400000u >> e;
	return asfloat(u & ~m);
}

float roundevenf(float x)
{
	return (float)roundeven(x);
}

float rintf(float x)
{
	uint32_t u = asuint(x);
	int e = (int)(u >> 23 & 0xff);
	if (e >= 0x7f + 23) return e == 0xff ? x + x : x;
	float y = u >> 31 ? (x - 0x1p23f) + 0x1p23f : (x + 0x1p23f) - 0x1p23f;
	return y == 0 ? asfloat(u & 0x80000000u) : y;
}

float nearbyintf(float x)
{
	int saved = fetestexcept(FE_INEXACT);
	float y = rintf(x);
	if (!saved) feclearexcept(FE_INEXACT);
	return y;
}

long lroundf(float x)
{
	return lround(x);
}

long long llroundf(float x)
{
	return lround(x);
}
