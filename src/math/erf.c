/*
 * lib-spfxd — erf and erfc.
 *
 *   |x| < 0.84375:   erf(x) = x P(x^2), leading term in dd
 *   0 <= x < 6.5:    erfc(x) = exp(-x^2) R_k(x - a_k) on eight pieces
 *   6.5 <= x < 27.3: erfc(x) = exp(-x^2) / x * G(1/x^2)
 * exp(-x^2) uses the dd exp kernel on the exact square, and the last
 * Horner steps of each polynomial are done in dd, so cancellation inside
 * the polynomials does not cost accuracy.  erf = 1 - erfc above 0.84375,
 * where erfc < 0.24, and erfc(-x) = 2 - erfc(x).
 */
#include <math.h>
#include "fp.h"

/* c[0] + c[1] t + ... + c[n] t^n with the last two steps in dd; lo[]
 * holds the low parts of c[0] and c[1] */
static dd_t poly_dd2(const double *c, const double *lo, int n, double t)
{
	double u2 = poly(c + 2, n - 2, t);
	dd_t u1 = two_prod(t, u2);
	u1 = dd_add(u1, (dd_t){ c[1], lo[1] });
	dd_t u0 = dd_mul_d(u1, t);
	return dd_add(u0, (dd_t){ c[0], lo[0] });
}

/* erfc(a) for 0 <= a < 27.3 as e * 2^k */
static dd_t erfc_pos(double a, int *k)
{
	dd_t sq = two_prod(a, a);
	dd_t e = __exp_dd_kernel(dd_neg(sq), k);
	if (a < 6.5) {
		int i = 0;
		while (a >= __erfc_bounds[i + 1]) i++;
		dd_t p = poly_dd2(__erfc_coef + __erfc_start[i], __erfc_lo + 2 * i, __erfc_degree[i], a - __erfc_bounds[i]);
		return dd_mul(e, p);
	}
	double u = 1.0 / (a * a);
	dd_t g = dd_add_d(dd_from(u * poly(__erfc_asym + 1, 13, u)), __erfc_asym[0]);
	return dd_div(dd_mul(e, g), dd_from(a));
}

double erf(double x)
{
	double a = fabs(x), r;
	uint32_t top = TOP12(a);
	if (top >= 0x7ff) {
		if (__builtin_isnan(x)) return x + x;
		return x < 0 ? -1.0 : 1.0;
	}
	if (top < 0x3e3) {                         /* |x| < 2^-28 */
		/* 2x/sqrt(pi); scale to keep subnormal x accurate */
		if (top < 0x010) return 0.125 * (8.0 * x + x * (8.0 * (__two_over_sqrtpi[0] - 1.0)));
		return x + x * (__two_over_sqrtpi[0] - 1.0);
	}
	if (a < 0.84375) {
		double z = x * x;
		dd_t p = two_prod(x, __two_over_sqrtpi[0]);
		p.lo += x * __two_over_sqrtpi[1] + x * z * poly(__erf_poly + 1, 10, z);
		return p.hi + p.lo;
	}
	if (a >= 6.0) {
		r = 1.0 - fp_barrier(0x1p-1000);
	} else {
		int k;
		dd_t c = erfc_pos(a, &k);
		double s = pow2i(k);
		dd_t d = dd_add_d((dd_t){ -c.hi * s, -c.lo * s }, 1.0);
		r = d.hi + d.lo;
	}
	return x < 0 ? -r : r;
}

double erfc(double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (__builtin_isinf(x)) return x < 0 ? 2.0 : 0.0;
	double a = fabs(x);
	if (a < 0x1p-56) return 1.0 - x;
	if (x < 0) {
		if (x < -6.0) return 2.0 - fp_barrier(0x1p-1000);
		int k;
		dd_t c = erfc_pos(a, &k);
		double s = pow2i(k);
		dd_t d = dd_add_d((dd_t){ -c.hi * s, -c.lo * s }, 2.0);
		return d.hi + d.lo;
	}
	if (x >= 27.3) return __math_uflow(0);
	int k;
	dd_t c = erfc_pos(a, &k);
	return __exp_finish(c, k, 0);
}
