/*
 * lib-spfxd — lgamma, lgamma_r, gamma, tgamma.
 *
 * lgamma on (0, inf), all in double-double:
 *   [0.75, 1.25]  lgamma(1+t) = t P1(t)   (exact t = x - 1, degree 20)
 *   [1.25, 2.75]  lgamma(2+t) = t P2(t)   (exact t = x - 2, degree 27)
 *   (0, 0.75)     lgamma(x+1) - log(x)
 *   (2.75, 8)     shift down into [1.75, 2.75] with a dd product
 *   [8, inf)      Stirling series with 10 terms
 * Expanding around the zeros at 1 and 2 keeps the relative error small
 * there.  Negative x uses the reflection formula; near the zeros of
 * lgamma on the negative axis only absolute accuracy is guaranteed.
 *
 * tgamma: exp of the dd lgamma of a shifted argument in [0.75, 2.75]
 * times/divided by a dd product for x <= 30, Stirling + dd exp above,
 * reflection with sin(pi x) in dd for x < 0.  Results are within ~1 ulp.
 */
#include <math.h>
#include "fp.h"

int signgam;

/* t P(t) for a dd t, with P's last two Horner steps in dd and the low
 * part of t entering through the derivative (P + t P'). */
static dd_t tpoly(const double *c, int n, dd_t t)
{
	double th = t.hi;
	double u2 = poly(c + 2, n - 2, th);
	dd_t p = dd_add_d(two_prod(th, u2), c[1]);
	p = dd_add_d(dd_mul_d(p, th), c[0]);
	dd_t r = dd_mul_d(p, th);
	if (t.lo != 0) {
		double d = 0;                       /* derivative of t P(t) */
		for (int i = n; i >= 0; i--) d = d * th + (i + 1) * c[i];
		r = dd_add_d(r, t.lo * d);
	}
	return r;
}

static dd_t log_of_dd(dd_t w)
{
	dd_t l = __log_dd(w.hi);
	if (w.lo != 0) l = dd_add_d(l, w.lo / w.hi);
	return l;
}

/* lgamma(x) for x in [0.75, 2.75] (t passed as an exact dd offset) */
static dd_t lg_core(double x)
{
	if (x <= 1.25) return tpoly(__lgamma1_poly, 20, dd_from(x - 1.0));
	return tpoly(__lgamma2_poly, 27, dd_from(x - 2.0));
}

/* lgamma for finite x > 0 as a dd (x < 2^52), or a plain double in hi
 * for larger x */
static dd_t lg_pos(double x)
{
	if (x < 0x1p-70) return dd_neg(__log_dd(x));
	if (x < 0.75) {
		dd_t g;
		if (x <= 0.25) g = tpoly(__lgamma1_poly, 20, dd_from(x));
		else g = tpoly(__lgamma2_poly, 27, two_sum(x, -1.0));
		return dd_add(g, dd_neg(__log_dd(x)));
	}
	if (x <= 2.75) return lg_core(x);
	if (x < 8.0) {
		int n = (int)(x - 2.75) + 1;
		double y = x - n;                   /* exact, in (1.75, 2.75] */
		dd_t p = dd_from(y);
		for (int i = 1; i < n; i++) p = dd_mul_d(p, y + i);
		return dd_add(lg_core(y), log_of_dd(p));
	}
	if (x >= 0x1p52) return dd_from(x * (log(x) - 1.0));
	double w = 1.0 / x;
	double series = w * poly(__stirling, 9, w * w);
	dd_t l = dd_mul_d(__log_dd(x), x - 0.5);
	l = dd_add_d(l, -x);
	l = dd_add(l, dd_c(__half_log_2pi));
	return dd_add_d(l, series);
}

double __lgamma_r(double x, int *sg)
{
	*sg = 1;
	if (!__builtin_isfinite(x)) return x * x;
	if (x == 0) {
		if (__builtin_signbit(x)) *sg = -1;
		return __math_divzero(0);
	}
	if (x > 0) {
		if (x > 2.556348163871e305) return __math_oflow(0);
		dd_t r = lg_pos(x);
		return r.hi + r.lo;
	}
	/* x < 0 */
	if (floor(x) == x) return __math_divzero(0);   /* pole */
	double a = -x;
	if (a < 0x1p-70) {
		*sg = -1;
		dd_t r = dd_neg(__log_dd(a));
		return r.hi + r.lo;
	}
	if (a >= 0x1p52) return __math_divzero(0);
	/* Gamma(x) = -pi / (x sin(pi x) Gamma(-x)) */
	dd_t s = __sinpi_dd(x);
	if (s.hi < 0) *sg = -1;
	dd_t xs = dd_mul_d(s, a);
	if (xs.hi < 0) xs = dd_neg(xs);
	dd_t r = dd_add(dd_c(__log_pi), dd_neg(log_of_dd(xs)));
	r = dd_add(r, dd_neg(lg_pos(a)));
	return __math_check_oflow(r.hi + r.lo);
}

double lgamma_r(double x, int *sg)
{
	return __lgamma_r(x, sg);
}

double lgamma(double x)
{
	return __lgamma_r(x, &signgam);
}

double gamma(double x)
{
	return __lgamma_r(x, &signgam);
}

/* Gamma(y) for 0 < y <= 185 as e * 2^k */
static dd_t tg_pos(double y, int *k)
{
	if (y <= 30.0) {
		dd_t g;
		if (y < 0.75) {
			/* Gamma(y) = Gamma(y + m) / (y (y+1) ... (y+m-1)) */
			dd_t lg = y <= 0.25 ? tpoly(__lgamma1_poly, 20, dd_from(y))
			                    : tpoly(__lgamma2_poly, 27, two_sum(y, -1.0));
			g = __exp_dd_kernel(lg, k);
			g = dd_div(g, dd_from(y));
			return g;
		}
		int n = y > 2.75 ? (int)(y - 2.75) + 1 : 0;
		double z = y - n;                   /* exact, in [0.75, 2.75] */
		g = __exp_dd_kernel(lg_core(z), k);
		for (int i = 0; i < n; i++) g = dd_mul_d(g, z + i);
		return g;
	}
	double w = 1.0 / y;
	double series = w * poly(__stirling, 9, w * w);
	dd_t l = dd_mul_d(__log_dd(y), y - 0.5);
	l = dd_add_d(l, -y);
	l = dd_add(l, dd_c(__half_log_2pi));
	l = dd_add_d(l, series);
	return __exp_dd_kernel(l, k);
}

double tgamma(double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (__builtin_isinf(x)) return x > 0 ? x : __math_invalid(x);
	if (x == 0) return __math_divzero(__builtin_signbit(x) ? 1 : 0);
	double a = fabs(x);
	if (a < 0x1p-54) return __math_check_oflow(1.0 / x);
	if (x > 0) {
		if (x > 171.62437695630272) return __math_oflow(0);
		int k;
		dd_t g = tg_pos(x, &k);
		return __exp_finish(g, k, 0);
	}
	if (floor(x) == x) return __math_invalid(x);
	dd_t s = __sinpi_dd(x);
	int neg = s.hi < 0;                     /* sign of Gamma(x) */
	if (x < -184.0) return __math_uflow((uint32_t)neg);
	/* Gamma(x) = -pi / (x sin(pi x) Gamma(-x)) = pi / (|x sin(pi x)| Gamma(-x)) */
	int k;
	dd_t g = tg_pos(a, &k);
	dd_t xs = dd_mul_d(s, a);
	if (xs.hi < 0) xs = dd_neg(xs);
	dd_t den = dd_mul(xs, g);
	/* normalize den to avoid overflow in the division */
	int e;
	(void)frexp(den.hi, &e);
	double sc = scalbn(1.0, -e);
	den.hi *= sc;
	den.lo *= sc;
	dd_t q = dd_div(dd_c(__pi_dd), den);
	return __exp_finish(q, -k - e, neg);
}
