/*
 * lib-spfxd — exp, exp2, exp10 (pow10), expm1 and the shared dd kernel.
 *
 * Method: x = k ln2/32 + r with |r| <= ln2/64, so
 *     exp(x) = 2^(k>>5) * 2^((k&31)/32) * exp(r).
 * 2^(j/32) comes from a double-double table, exp(r) - 1 =
 * r + r^2/2 + r^3 P(r) with P a degree-4 minimax polynomial, and the
 * reduction is done in double-double so the input may itself be a dd
 * (pow, exp10 and erfc pass y*log(x), x*ln10 and -x^2 that way).  The
 * kernel result has relative error below 2^-63, so the final rounding
 * is wrong only in rare near-halfway cases (< 0.51 ulp).
 */
#include <errno.h>
#include <math.h>
#include "fp.h"

dd_t __exp_dd_kernel(dd_t x, int *scale)
{
	int k = iround(x.hi * __exp_inv_ln2_32);
	double kd = (double)k;
	/* k*ln2_32[0] is exact (37-bit constant, |k| < 2^16) and so is the
	 * subtraction (the operands are within a factor of two) */
	double rh = x.hi - kd * __exp_ln2_32[0];
	dd_t r = two_sum(rh, x.lo - kd * __exp_ln2_32[1]);

	/* exp(r) - 1 = r.hi + lo */
	double r2 = r.hi * r.hi;
	double lo = r.lo + r.hi * r.lo + 0.5 * r2 + r2 * r.hi * poly(__expm1_poly, 4, r.hi);

	int j = k & 31;
	*scale = k >> 5;          /* arithmetic shift: floor(k / 32) */
	double th = __exp_tab[2 * j], tl = __exp_tab[2 * j + 1];
	/* T * (1 + r.hi + lo) */
	dd_t p = two_prod(th, r.hi);
	dd_t s = two_sum(th, p.hi);
	s.lo += p.lo + tl + th * lo + tl * (r.hi + lo);
	return fast_two_sum(s.hi, s.lo);
}

/* Scale a dd by 2^k and round once to double, setting errno on overflow
 * or underflow. */
double __exp_finish(dd_t e, int k, int sign)
{
	if (sign) e = dd_neg(e);
	if (k > -1022) {
		double y = e.hi + e.lo;
		if (k > 1023) return __math_check_oflow(y * 2.0 * pow2i(k - 1));
		return __math_check_oflow(y * pow2i(k));
	}
	/* subnormal (or nearly) result: scale both parts exactly into the
	 * normal range, combine, then apply the final factor with a single
	 * rounding */
	double h = e.hi * pow2i(k + 1000), l = e.lo * pow2i(k + 1000);
	double y = (h + l) * 0x1p-1000;
	return __math_check_uflow(y);
}

double __exp_dd(dd_t x, int sign)
{
	if (x.hi > 709.8) return __math_oflow(sign);
	if (x.hi < -745.2) return __math_uflow(sign);
	int k;
	dd_t e = __exp_dd_kernel(x, &k);
	return __exp_finish(e, k, sign);
}

double exp(double x)
{
	uint32_t top = TOP12(x) & 0x7ff;
	if (unlikely(top >= 0x408)) {            /* |x| >= 512, inf or nan */
		if (__builtin_isnan(x)) return x + x;
		if (__builtin_isinf(x)) return x > 0 ? x : 0.0;
		if (x > 709.782712893384) return __math_oflow(0);
		if (x < -745.1332191019412) return __math_uflow(0);
	} else if (unlikely(top < 0x3c9)) {      /* |x| < 2^-54 */
		return 1.0 + x;
	}
	return __exp_dd(dd_from(x), 0);
}

double exp2(double x)
{
	uint32_t top = TOP12(x) & 0x7ff;
	if (unlikely(top >= 0x409)) {            /* |x| >= 1024, inf or nan */
		if (__builtin_isnan(x)) return x + x;
		if (__builtin_isinf(x)) return x > 0 ? x : 0.0;
		if (x > 0) return __math_oflow(0);
		if (x <= -1075.0) return __math_uflow(0);
	} else if (unlikely(top < 0x3c9)) {
		return 1.0 + x;
	}
	/* exact powers of two are common enough to deserve a fast exact path */
	if (x == (double)(int)x && x >= -1022 && x <= 1023) return pow2i((int)x);
	dd_t t = two_prod(x, __ln2[0]);
	t.lo += x * __ln2[1];
	return __exp_dd(fast_two_sum(t.hi, t.lo), 0);
}

double exp10(double x)
{
	uint32_t top = TOP12(x) & 0x7ff;
	if (unlikely(top >= 0x407)) {            /* |x| >= 256, inf or nan */
		if (__builtin_isnan(x)) return x + x;
		if (__builtin_isinf(x)) return x > 0 ? x : 0.0;
		if (x > 308.3) return __math_oflow(0);
		if (x < -324.0) return __math_uflow(0);
	} else if (unlikely(top < 0x3c9)) {
		return 1.0 + x;
	}
	dd_t t = two_prod(x, __ln10[0]);
	t.lo += x * __ln10[1];
	return __exp_dd(fast_two_sum(t.hi, t.lo), 0);
}

double pow10(double x)
{
	return exp10(x);
}

double expm1(double x)
{
	uint32_t top = TOP12(x) & 0x7ff;
	if (unlikely(top >= 0x404)) {            /* |x| >= 32, inf or nan */
		if (__builtin_isnan(x)) return x + x;
		if (__builtin_isinf(x)) return x > 0 ? x : -1.0;
		if (x > 709.782712893384) return __math_oflow(0);
		if (x < -38.0) return fp_barrier(0x1p-1000) - 1.0;   /* -1, inexact */
	}
	if (top < 0x3c9) {                       /* |x| < 2^-54 */
		if (x != 0) fp_force_eval(x * x + 0x1p-1000);
		return x;
	}
	double ax = fabs(x);
	if (ax < 0.0108) {
		/* direct series: x + x^2/2 + x^3 P(x) */
		double x2 = x * x;
		return x + (0.5 * x2 + x2 * x * poly(__expm1_poly, 4, x));
	}
	int k;
	dd_t e = __exp_dd_kernel(dd_from(x), &k);
	if (k > 1020) {
		/* exp(x) - 1 == exp(x) in double */
		return __exp_finish(e, k, 0);
	}
	double sc = pow2i(k);
	dd_t r = two_sum(e.hi * sc, -1.0);
	r.lo += e.lo * sc;
	return r.hi + r.lo;
}
