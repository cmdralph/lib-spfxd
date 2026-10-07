/*
 * lib-spfxd — fast paths for the most used libm functions.
 *
 * Each function first evaluates a cheap approximation hi + lo with a
 * proven bound eps on its error, then applies Ziv's rounding test: if
 * hi + (lo - eps) and hi + (lo + eps) round to the same double, the exact
 * result rounds to it as well (rounding is monotonic), so that double is
 * returned.  Otherwise the function falls through to its accurate
 * double-double path.  The fast path therefore never changes a result;
 * it only skips work when the answer is already certain.  The bounds
 * below hold in every rounding mode (each operation then errs by at most
 * one ulp instead of half an ulp).
 *
 * The float functions use the same cores: a double approximation with a
 * small relative error is rounded to float unless it lies too close to a
 * float rounding boundary (float_round_ok).
 */
#ifndef _SPFXD_FASTPATH_H
#define _SPFXD_FASTPATH_H

#include "fp.h"

extern hidden const double __exp128_tab[256], __exp128_ln2[2], __exp128_inv, __exp128_poly[5];

/*
 * exp(xh + xl) = 2^s (hi + lo) for |xh| <= 709.8 and |xl| <= 2^-40.
 *
 * x = k ln2/128 + r, k = round(x 128/ln2), |r| <= ln2/256 (+ 2^-40):
 *   rh = xh - k C1           exact: C1 has 35 bits and |k| < 2^17, and the
 *                            subtraction is exact by Sterbenz's lemma
 *   rl = xl - k C2           |rl| <= 2^-25, error < 2^-78
 *   q  = rl + r^2 P(r)       exp(r) - 1 - rh, |q| < 2^-17, error < 2^-69
 *   exp(x) = 2^(k>>7) T (1 + rh + q),  T = th + tl = 2^((k&127)/128)
 *   lo = th rh + (th q + tl (1 + r))
 * |th rh| < 2^-7.4, so its rounding errs by at most 2^-60 (directed
 * modes; 2^-61 to nearest), as does the final addition (|lo| < 2^-7.4);
 * the dropped tl q is < 2^-69.  Total error < 2^-59 + 2^-66.  With the
 * rounding of lo +- eps (< 2^-60) the test needs eps >= 1.5 * 2^-59;
 * EXP_EPS = 2^-58 leaves margin.  hi = th is in [1, 2).
 */
#define EXP_EPS 0x1p-58

static always_inline int exp_fast_core(double xh, double xl, double *hi, double *lo)
{
	int k = iround(xh * __exp128_inv);
	double kd = (double)k;
	double rh = xh - kd * __exp128_ln2[0];
	double rl = xl - kd * __exp128_ln2[1];
	double r = rh + rl;
	double r2 = r * r;
	const double *c = __exp128_poly;
	double q = rl + r2 * ((c[0] + r * c[1]) + r2 * ((c[2] + r * c[3]) + r2 * c[4]));
	int j = k & 127;
	double th = __exp128_tab[2 * j], tl = __exp128_tab[2 * j + 1];
	*hi = th;
	*lo = th * rh + (th * q + tl * (1.0 + r));
	return k >> 7;                          /* floor(k / 128) */
}

/* hi + lo rounded into *res when the rounding is certain (compared as bit
 * patterns: one integer compare, no unordered-compare flag handling). */
static always_inline int ziv_round(double hi, double lo, double eps, double *res)
{
	double a = hi + (lo - eps), b = hi + (lo + eps);
	*res = a;
	return asuint64(a) == asuint64(b);
}

/* a * 2^s for a normal result: add s to the exponent field. */
static always_inline double scale_normal(double a, int s)
{
	return asdouble(asuint64(a) + ((uint64_t)(int64_t)s << 52));
}

/*
 * log(m) for x = 2^k m, x positive normal, m in [0.75, 1.5): returns k and
 * log(m) = mh + ml, with error below
 *     LOG_ERR(r2) = 2^-50 r2 + 2^-80
 * where r2 = r^2 is returned too.  (c = __log_inv[i] approximates 1/m)
 *   m c - 1 = rh + rl exactly (two_prod; rh = p - 1 is exact by Sterbenz)
 *   log(m) = -log(c) + log1p(r),  log1p(r) = r - r^2/2 + r^3 P(r)
 *   mh + e = logc_hi + rh exactly (|logc_hi| >= |rh| unless logc = 0, in
 *            which case the sum is rh itself)
 *   ml = e + (logc_lo + rl) - r^2/2 + r^3 P(r)
 * |r| <= 2^-7, so |r^2/2| <= 2^-15; the three roundings in forming ml are
 * each below one ulp of the running sum, i.e. below 2^-52 (r^2/2) plus
 * terms at the 2^-100 level; r^2 itself is rounded once (2^-53 r^2/2).
 * When m is next to 1 (c = 1) every term scales with r, so the bound is
 * relative to the result there as it must be.
 */
static always_inline int log_fast_core(double x, double *mh, double *ml, double *r2p)
{
	uint64_t u = asuint64(x);
	uint64_t tmp = u - 0x3fe8000000000000ULL;
	int k = (int)((int64_t)tmp >> 52);
	int i = (int)(tmp >> 45) & 127;
	double m = asdouble(u - ((uint64_t)(int64_t)k << 52));
	double c = __log_inv[i];
	dd_t p = two_prod(m, c);
	double rh = p.hi - 1.0, rl = p.lo;
	double r = rh + rl;
	double r2 = r * r;
	dd_t h = fast_two_sum(__log_logc[2 * i], rh);
	*mh = h.hi;
	*ml = h.lo + ((__log_logc[2 * i + 1] + rl) + r2 * (r * poly(__log1p_poly, 7, r) - 0.5));
	*r2p = r2;
	return k;
}

#define LOG_ERR(r2) (0x1p-50 * (r2) + 0x1p-80)

/*
 * log(x) = Lh + Ll for positive normal x, accurate enough for pow:
 * returns a bound on |Lh + Ll - log x|.  As log_fast_core, but r^2/2 is
 * taken exactly (rh^2 by two_prod) and the large parts are combined with
 * error-free transformations, so only small terms are rounded:
 *   log x = k ln2hi + logc_hi + rh - rh^2/2                 (exact sums)
 *         + k ln2lo + logc_lo + rl - rh rl - (rh^2)lo/2 + r^3 P(r)
 * Each of the (at most 10) roundings in the small sum errs by less than
 * 2^-52 times the sum of the magnitudes S, giving 2^-48.6 S; P's own
 * approximation error (2^-59 r^3) is inside that, and the dd constants'
 * representation errors are below 2^-100 |Lh|.
 */
static always_inline dd_t log_acc_core(double x, double *err)
{
	uint64_t u = asuint64(x);
	uint64_t tmp = u - 0x3fe8000000000000ULL;
	int k = (int)((int64_t)tmp >> 52);
	int i = (int)(tmp >> 45) & 127;
	double m = asdouble(u - ((uint64_t)(int64_t)k << 52));
	double c = __log_inv[i];
	dd_t p = two_prod(m, c);
	double rh = p.hi - 1.0, rl = p.lo;
	double r = rh + rl;
	dd_t sq = two_prod(rh, rh);
	double kd = (double)k;
	dd_t a = fast_two_sum(__log_logc[2 * i], rh);
	dd_t b = two_sum(a.hi, -0.5 * sq.hi);
	dd_t h = two_sum(kd * __log_ln2[0], b.hi);
	double t1 = kd * __log_ln2[1], t2 = __log_logc[2 * i + 1];
	double t3 = rl - rh * rl - 0.5 * sq.lo;
	double t4 = r * r * r * poly(__log1p_poly, 7, r);
	double lo = ((a.lo + b.lo) + (h.lo + t1)) + ((t2 + t3) + t4);
	double sabs = fabs(a.lo) + fabs(b.lo) + fabs(h.lo) + fabs(t1) + fabs(t2) + fabs(rl) + 0x1p-7 * fabs(rl)
	            + fabs(sq.lo) + fabs(t4);
	*err = 0x1p-48 * sabs + 0x1p-100 * fabs(h.hi);
	return (dd_t){ h.hi, lo };
}

/*
 * Rounding test for a float result approximated by the double y with a
 * relative error below 2^-49 (a few double ulps): y may be rounded to
 * float unless its 29 low-order significand bits are within 2^4 of a
 * float rounding boundary (a float, or a midpoint between two floats).
 * 2^-49 relative is at most 16 double ulps; the window is 64 ulps wide on
 * each side.  The caller guarantees y is in the normal float range.
 */
static always_inline int float_round_ok(double y)
{
	uint64_t low = (asuint64(y) + 64) & 0x0fffffff;
	return low > 128;
}

#endif
