/*
 * lib-spfxd — sin, cos, tan, sincos and their reduced-argument kernels.
 *
 * After reduction x = n pi/2 + r (r a double-double, |r| <= pi/4):
 *     sin(r) = r + r^3 S(r^2)            S: degree 6, minimax
 *     cos(r) = 1 - r^2/2 + r^4 C(r^2)    C: degree 6, minimax
 * with r^2/2 formed exactly so 1 - r^2/2 loses nothing, and the low part
 * of r entering through the derivative.  tan is sin/cos in dd.
 */
#include <math.h>
#include "fp.h"

dd_t __sin_dd(dd_t r)
{
	double z = r.hi * r.hi;
	double t = r.hi * z * poly(__sin_poly, 6, z) + r.lo * (1.0 - 0.5 * z);
	return fast_two_sum(r.hi, t);
}

double __sin_kernel(dd_t r)
{
	double z = r.hi * r.hi;
	return r.hi + (r.hi * z * poly(__sin_poly, 6, z) + r.lo * (1.0 - 0.5 * z));
}

dd_t __cos_dd(dd_t r)
{
	dd_t z = two_prod(r.hi, r.hi);
	double hz = 0.5 * z.hi;
	double w = 1.0 - hz;
	double e = (1.0 - w) - hz;            /* exact rounding error of w */
	double lo = e - 0.5 * z.lo + z.hi * z.hi * poly(__cos_poly, 6, z.hi) - r.hi * r.lo;
	return fast_two_sum(w, lo);
}

double __cos_kernel(dd_t r)
{
	dd_t c = __cos_dd(r);
	return c.hi + c.lo;
}

double sin(double x)
{
	uint32_t top = TOP12(x) & 0x7ff;
	if (top < 0x3e5) {                     /* |x| < 2^-26 */
		if (top < 0x010) fp_force_eval(x * x);   /* underflow for subnormal */
		if (x != 0) fp_force_eval(x + 0x1p-1000);
		return x;
	}
	if (top >= 0x7ff) return __math_invalid(x);
	dd_t r;
	int n = __rem_pio2(x, &r);
	switch (n & 3) {
	case 0: return __sin_kernel(r);
	case 1: return __cos_kernel(r);
	case 2: return -__sin_kernel(r);
	default: return -__cos_kernel(r);
	}
}

double cos(double x)
{
	uint32_t top = TOP12(x) & 0x7ff;
	if (top < 0x3e4) {                     /* |x| < 2^-27 */
		fp_force_eval(x + 0x1p-1000);
		return 1.0;
	}
	if (top >= 0x7ff) return __math_invalid(x);
	dd_t r;
	int n = __rem_pio2(x, &r);
	switch (n & 3) {
	case 0: return __cos_kernel(r);
	case 1: return -__sin_kernel(r);
	case 2: return -__cos_kernel(r);
	default: return __sin_kernel(r);
	}
}

void sincos(double x, double *s, double *c)
{
	uint32_t top = TOP12(x) & 0x7ff;
	if (top < 0x3e4) {
		if (top < 0x010) fp_force_eval(x * x);
		if (x != 0) fp_force_eval(x + 0x1p-1000);
		*s = x;
		*c = 1.0;
		return;
	}
	if (top >= 0x7ff) {
		*s = *c = __math_invalid(x);
		return;
	}
	dd_t r;
	int n = __rem_pio2(x, &r);
	double sv = __sin_kernel(r), cv = __cos_kernel(r);
	switch (n & 3) {
	case 0: *s = sv; *c = cv; break;
	case 1: *s = cv; *c = -sv; break;
	case 2: *s = -sv; *c = -cv; break;
	default: *s = -cv; *c = sv; break;
	}
}

double tan(double x)
{
	uint32_t top = TOP12(x) & 0x7ff;
	if (top < 0x3e5) {
		if (top < 0x010) fp_force_eval(x * x);
		if (x != 0) fp_force_eval(x + 0x1p-1000);
		return x;
	}
	if (top >= 0x7ff) return __math_invalid(x);
	dd_t r;
	int n = __rem_pio2(x, &r);
	dd_t s = __sin_dd(r), c = __cos_dd(r);
	dd_t t = (n & 1) ? dd_neg(dd_div(c, s)) : dd_div(s, c);
	return t.hi + t.lo;
}

/* sin(pi x): x = k/2 + r exactly with |r| <= 1/4 */
dd_t __sinpi_dd(double x)
{
	double ax = fabs(x);
	if (ax >= 0x1p52) return dd_from(0.0);
	double kd, rr;
	if (ax >= 0x1p51) {            /* x is a multiple of 1/2 */
		kd = 2.0 * x;
		rr = 0.0;
	} else {
		kd = (double)(int64_t)(2.0 * x + (x < 0 ? -0.5 : 0.5));
		rr = x - 0.5 * kd;
	}
	int64_t k = (int64_t)kd;
	dd_t r = two_prod(rr, __pi_dd[0]);
	r.lo += rr * __pi_dd[1];
	r = fast_two_sum(r.hi, r.lo);
	switch (k & 3) {
	case 0: return __sin_dd(r);
	case 1: return __cos_dd(r);
	case 2: return dd_neg(__sin_dd(r));
	default: return dd_neg(__cos_dd(r));
	}
}

double __sinpi(double x)
{
	dd_t r = __sinpi_dd(x);
	return r.hi + r.lo;
}
