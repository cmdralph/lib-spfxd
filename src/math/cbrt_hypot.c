/*
 * lib-spfxd — cbrt and hypot.
 *
 * cbrt: an exponent-divided-by-three bit estimate, Newton iterations in
 * double, then one correction step with y^3 computed exactly in dd so the
 * final rounding sees a value good to ~2^-100.
 *
 * hypot: operands are scaled by a power of two into a safe range, the sum
 * of squares is formed exactly as a dd and its dd square root rounded
 * once, so the result is correctly rounded except in rare near-halfway
 * cases.  Infinity wins over NaN as Annex F requires.
 */
#include <errno.h>
#include <math.h>
#include "fp.h"

double cbrt(double x)
{
	uint64_t u = asuint64(x);
	uint64_t sign = u & (1ULL << 63);
	double a = fabs(x);
	if (!(u << 1) || EXP_BITS(x) == 0x7ff) return x + x;
	double scale = 1.0;
	if (EXP_BITS(x) == 0) {                 /* subnormal: scale by 2^54 */
		a *= 0x1p54;
		scale = 0x1p-18;
	} else if (EXP_BITS(x) > 0x3ff + 600) { /* y^3 would overflow */
		a *= 0x1p-999;
		scale = 0x1p333;
	}
	/* estimate: divide the biased exponent field (with mantissa) by 3 */
	int64_t ua = (int64_t)asuint64(a) - (1023LL << 52);
	double y = asdouble((uint64_t)(ua / 3 + (1023LL << 52)));
	for (int i = 0; i < 4; i++) y = y - (y * y * y - a) / (3.0 * y * y);
	/* y <- y - (y^3 - a) / (3 y^2) with y^3 - a exact-ish in dd */
	dd_t y3 = dd_mul_d(two_prod(y, y), y);
	dd_t d = dd_add_d(y3, -a);
	double corr = (d.hi + d.lo) / (3.0 * y * y);
	double r = y - corr;
	r *= scale;
	return asdouble(asuint64(r) | sign);
}

double hypot(double x, double y)
{
	if (__builtin_isinf(x) || __builtin_isinf(y)) return __builtin_inf();
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	double a = fabs(x), b = fabs(y);
	if (a < b) { double t = a; a = b; b = t; }
	if (b == 0) return a;
	int ea = EXP_BITS(a), eb = EXP_BITS(b);
	if (ea - eb > 54) return a + b;         /* b only affects rounding */
	double sc = 1.0;
	if (ea > 0x3ff + 500) { a *= 0x1p-600; b *= 0x1p-600; sc = 0x1p600; }
	else if (eb < 0x3ff - 500) { a *= 0x1p600; b *= 0x1p600; sc = 0x1p-600; }
	dd_t s = dd_add(two_prod(a, a), two_prod(b, b));
	dd_t r = dd_sqrt(s);
	double res = (r.hi + r.lo) * sc;
	if (__builtin_isinf(res)) errno = ERANGE;
	return res;
}
