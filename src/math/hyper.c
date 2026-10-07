/*
 * lib-spfxd — sinh, cosh, tanh, asinh, acosh, atanh.
 *
 * Small arguments use odd minimax polynomials (sinh, tanh).  Elsewhere the
 * functions are formed from the exp kernel's double-double result, so
 * E - 1/E and (E^2 - 1)/(E^2 + 1) are evaluated without losing bits.
 * Large arguments use exp(|x| - ln2), which overflows exactly where the
 * true result does.  The inverse functions are logarithms of dd
 * expressions, arranged so that the logarithm sees an argument close to 1
 * only when it is computed exactly (log uses c = 1 there).
 */
#include <math.h>
#include "fp.h"

/* exp(a) as a dd scaled into range (a <= 23) */
static dd_t exp_scaled(double a)
{
	int k;
	dd_t e = __exp_dd_kernel(dd_from(a), &k);
	double s = pow2i(k);
	return (dd_t){ e.hi * s, e.lo * s };
}

static double log_of_dd(dd_t w)
{
	dd_t l = __log_dd(w.hi);
	if (w.lo != 0) l = dd_add_d(l, w.lo / w.hi);
	return l.hi + l.lo;
}

double sinh(double x)
{
	double a = fabs(x), r;
	uint32_t top = TOP12(a);
	if (top < 0x3e5) {                          /* |x| < 2^-26 */
		if (top < 0x010) fp_force_eval(x * x);
		/* the true value lies strictly beyond/before x by far less than
		 * half an ulp: nudge so that directed rounding goes the right way
		 * (to nearest this is x itself); zero keeps its sign */
		return x == 0 ? x : x + x * 0x1p-60;
	}
	if (top >= 0x7ff) return x + x;
	if (a <= 0.5) {
		double z = x * x;
		return x + x * z * poly(__sinh_poly, 6, z);
	}
	if (a < 22.0) {
		dd_t e = exp_scaled(a);
		dd_t d = dd_add(e, dd_neg(dd_div((dd_t){ 1.0, 0.0 }, e)));
		r = 0.5 * (d.hi + d.lo);
	} else {
		r = __exp_dd(dd_add_d(dd_neg(dd_c(__ln2)), a), 0);
	}
	return x < 0 ? -r : r;
}

double cosh(double x)
{
	double a = fabs(x);
	uint32_t top = TOP12(a);
	if (top < 0x3e4) {
		fp_force_eval(x + 0x1p-1000);
		return 1.0;
	}
	if (top >= 0x7ff) return x * x;
	if (a < 22.0) {
		dd_t e = exp_scaled(a);
		dd_t s = dd_add(e, dd_div((dd_t){ 1.0, 0.0 }, e));
		return 0.5 * (s.hi + s.lo);
	}
	return __exp_dd(dd_add_d(dd_neg(dd_c(__ln2)), a), 0);
}

double tanh(double x)
{
	double a = fabs(x), r;
	uint32_t top = TOP12(a);
	if (top < 0x3e4) {
		if (top < 0x010) fp_force_eval(x * x);
		/* the true value lies strictly beyond/before x by far less than
		 * half an ulp: nudge so that directed rounding goes the right way
		 * (to nearest this is x itself); zero keeps its sign */
		return x == 0 ? x : x - x * 0x1p-60;
	}
	if (top >= 0x7ff) return __builtin_isnan(x) ? x + x : (x < 0 ? -1.0 : 1.0);
	if (a <= 0.55) {
		double z = x * x;
		return x + x * z * poly(__tanh_poly, 11, z);
	}
	if (a > 22.0) {
		r = 1.0 - fp_barrier(0x1p-1000);           /* 1, inexact */
	} else {
		dd_t e = exp_scaled(2.0 * a);
		dd_t t = dd_div(dd_add_d(e, -1.0), dd_add_d(e, 1.0));
		r = t.hi + t.lo;
	}
	return x < 0 ? -r : r;
}

double asinh(double x)
{
	double a = fabs(x), r;
	uint32_t top = TOP12(a);
	if (top < 0x3e5) {
		if (top < 0x010) fp_force_eval(x * x);
		/* the true value lies strictly beyond/before x by far less than
		 * half an ulp: nudge so that directed rounding goes the right way
		 * (to nearest this is x itself); zero keeps its sign */
		return x == 0 ? x : x - x * 0x1p-60;
	}
	if (top >= 0x7ff) return x + x;
	if (a > 0x1p28) {
		/* sqrt(a^2 + 1) == a to double precision: log(2a) */
		dd_t l = dd_add(__log_dd(a), dd_c(__ln2));
		r = l.hi + l.lo;
	} else {
		dd_t t = two_prod(a, a);
		dd_t s = dd_sqrt(dd_add_d(t, 1.0));
		r = log_of_dd(dd_add_d(s, a));
	}
	return x < 0 ? -r : r;
}

double acosh(double x)
{
	if (!__builtin_isgreaterequal(x, 1.0)) return __builtin_isnan(x) ? x + x : __math_invalid(x);
	if (x == 1.0) return 0.0;
	if (__builtin_isinf(x)) return x;
	if (x > 0x1p28) {
		dd_t l = dd_add(__log_dd(x), dd_c(__ln2));
		return l.hi + l.lo;
	}
	dd_t w;
	if (x < 2.0) {
		w = dd_mul_d(two_sum(x, 1.0), x - 1.0);    /* x - 1 exact */
	} else {
		w = dd_add_d(two_prod(x, x), -1.0);
	}
	return log_of_dd(dd_add_d(dd_sqrt(w), x));
}

double atanh(double x)
{
	double a = fabs(x);
	if (!__builtin_isless(a, 1.0)) {
		if (a == 1.0) return __math_divzero(__builtin_signbit(x) ? 1 : 0);
		return __builtin_isnan(x) ? x + x : __math_invalid(x);
	}
	uint32_t top = TOP12(a);
	if (top < 0x3e3) {
		if (top < 0x010) fp_force_eval(x * x);
		/* the true value lies strictly beyond/before x by far less than
		 * half an ulp: nudge so that directed rounding goes the right way
		 * (to nearest this is x itself); zero keeps its sign */
		return x == 0 ? x : x + x * 0x1p-60;
	}
	dd_t q = dd_div(two_sum(1.0, a), two_sum(1.0, -a));
	double r = 0.5 * log_of_dd(q);
	return x < 0 ? -r : r;
}
