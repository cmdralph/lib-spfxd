/*
 * lib-spfxd — fmod, remainder, remquo, drem (double and float).
 *
 * The remainder of two floating-point numbers is always exactly
 * representable, so these are computed exactly with integer arithmetic on
 * the significands: the dividend's significand is shifted left in steps
 * of at most 11 bits (keeping it below 2^64) and reduced modulo the
 * divisor's.  remquo tracks the low bits of the quotient along the way.
 *
 * The float versions call the double ones: float operands are exact
 * doubles and the exact remainder of floats is a float.
 */
#include <errno.h>
#include <math.h>
#include "fp.h"

/* For finite x, y with y != 0 and |x| >= |y|: |x| mod |y| as a double,
 * with the low 31 bits of the integer quotient in *q. */
static double mod_core(uint64_t ux, uint64_t uy, uint32_t *q)
{
	int ex = (int)(ux >> 52 & 0x7ff), ey = (int)(uy >> 52 & 0x7ff);
	uint64_t mx = ux & 0x000fffffffffffffULL, my = uy & 0x000fffffffffffffULL;
	/* normalize so bit 52 is set; ex/ey become the exponent of the lsb */
	if (ex) { mx |= 1ULL << 52; ex -= 1075; }
	else { int s = __builtin_clzll(mx) - 11; mx <<= s; ex = -1074 - s; }
	if (ey) { my |= 1ULL << 52; ey -= 1075; }
	else { int s = __builtin_clzll(my) - 11; my <<= s; ey = -1074 - s; }

	uint32_t quo = 0;
	int d = ex - ey;
	if (d < 0) {
		/* only possible when the significands differ: |x| >= |y| means
		 * this cannot happen with normalized operands except equal
		 * values, handled by the caller */
		*q = 0;
		return asdouble(ux);
	}
	quo = (uint32_t)(mx / my);
	mx %= my;
	while (d > 0 && mx) {
		int k = d > 11 ? 11 : d;
		uint64_t t = mx << k;
		quo = (quo << k) + (uint32_t)(t / my);
		mx = t % my;
		d -= k;
	}
	if (d > 0) quo <<= (d > 31 ? 31 : d);
	*q = quo & 0x7fffffff;
	if (!mx) return 0.0;
	/* mx * 2^ey is exactly representable */
	return scalbn((double)mx, ey);
}

double fmod(double x, double y)
{
	uint64_t ux = asuint64(x), uy = asuint64(y);
	uint64_t ax = ux & ~(1ULL << 63), ay = uy & ~(1ULL << 63);
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	if (ax >= 0x7ff0000000000000ULL || !ay) return __math_invalid(x * y);
	if (ax < ay) return x;
	uint32_t q;
	double r = mod_core(ax, ay, &q);
	return (ux >> 63) ? -r : r;
}

double remquo(double x, double y, int *quo)
{
	uint64_t ux = asuint64(x), uy = asuint64(y);
	uint64_t ax = ux & ~(1ULL << 63), ay = uy & ~(1ULL << 63);
	int neg = (int)(ux >> 63), qneg = (int)((ux ^ uy) >> 63);
	*quo = 0;
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	if (ax >= 0x7ff0000000000000ULL || !ay) return __math_invalid(x * y);
	double fx = asdouble(ax), fy = asdouble(ay), r;
	uint32_t q = 0;
	if (ax < ay) r = fx;
	else r = mod_core(ax, ay, &q);
	/* round the quotient to nearest (ties to even): compare r with fy - r,
	 * which is exact when r >= fy/2 and otherwise rounds to a value still
	 * greater than r */
	double other = fy - r;
	if (r > other || (r == other && (q & 1))) {
		r -= fy;
		q++;
	}
	q &= 0x7fffffff;
	*quo = qneg ? -(int)q : (int)q;
	if (r == 0) r = 0.0;
	return neg ? -r : r;
}

double remainder(double x, double y)
{
	int q;
	return remquo(x, y, &q);
}

double drem(double x, double y)
{
	return remainder(x, y);
}

float fmodf(float x, float y)
{
	return (float)fmod(x, y);
}

float remquof(float x, float y, int *quo)
{
	return (float)remquo(x, y, quo);
}

float remainderf(float x, float y)
{
	int q;
	return (float)remquo(x, y, &q);
}

float dremf(float x, float y)
{
	return remainderf(x, y);
}
