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
 * FMA clones.  The library is built for baseline x86-64, so each public
 * function with a fast path also has a clone compiled for FMA3, chosen at
 * run time from __cpu_features.  The cores below take a constant flag F:
 * with F set, a*b + c is one fused operation (FMADD) and exact products
 * come from one FMA.  Fused operations round once instead of twice, so
 * every error bound below still holds, and Ziv's test makes the returned
 * results identical either way.
 */
#if defined(__x86_64__)
#define FMA_TARGET __attribute__((__target__("fma"), __noinline__, __unused__))
#define HAVE_FMA() likely(__cpu_features & 8 /* CPU_FMA */)
#else
#define FMA_TARGET __attribute__((__noinline__, __unused__))
#define HAVE_FMA() 0
#endif

#define FMADD(F, a, b, c) ((F) ? __builtin_fma((a), (b), (c)) : (a) * (b) + (c))

static always_inline dd_t two_prod_f(int F, double a, double b)
{
	if (F) {
		double p = a * b;
		return (dd_t){ p, __builtin_fma(a, b, -p) };
	}
	return two_prod(a, b);
}

/* Estrin evaluation of c[0] + ... + c[n] x^n for the degrees used here */
static always_inline double poly_f(int F, const double *c, int n, double x)
{
	double x2 = x * x;
#define P1(i) FMADD(F, c[(i) + 1], x, c[i])
	switch (n) {
	case 4: return FMADD(F, x2 * x2, c[4], FMADD(F, x2, P1(2), P1(0)));
	case 5: return FMADD(F, x2 * x2, P1(4), FMADD(F, x2, P1(2), P1(0)));
	case 6: return FMADD(F, x2 * x2, FMADD(F, x2, c[6], P1(4)), FMADD(F, x2, P1(2), P1(0)));
	case 7: return FMADD(F, x2 * x2, FMADD(F, x2, P1(6), P1(4)), FMADD(F, x2, P1(2), P1(0)));
	default: return poly(c, n, x);
	}
#undef P1
}

/*
 * exp(xh + xl) = 2^s (hi + lo) for |xh| <= 709.8 and |xl| <= 2^-40.
 *
 * x = k ln2/128 + r with k = round(x 128/ln2) taken by the "magic shift"
 * (adding 1.5 * 2^52 leaves k in the low bits): no float/int conversion
 * on the critical path.  In directed rounding modes k may be off by one,
 * so |r| <= ln2/128 rather than ln2/256; the polynomial is accurate there
 * too (fitted on ln2/256 * 1.05, its error grows to < 2^-66 relative to
 * r^2 at twice the range), and the error bound below is stated in terms
 * of the actual magnitudes, so it holds either way.
 *   rh = xh - k C1           exact: C1 has 35 bits and |k| < 2^17, and the
 *                            subtraction is exact by Sterbenz's lemma
 *   rl = xl - k C2           |rl| <= 2^-25, error < 2^-78
 *   q  = rl + r^2 P(r)       exp(r) - 1 - rh, |q| < 2^-15, error < 2^-67
 *   exp(x) = 2^(k>>7) T (1 + rh + q),  T = th + tl = 2^((k&127)/128)
 *   lo = th rh + (th q + tl (1 + r))
 *   lo = (th q + tl (1 + r)) + th rh   (the two big terms formed apart,
 *                                       off the critical path)
 * The roundings of th rh and of the final sum are each at most one ulp of
 * |th rh| < 2 |rh| and of |lo| <= 2 |rh| + 2^-14, and everything else is
 * below 2^-65, so the error is below 2^-50 |rh| + 2^-65; the test's own
 * roundings of lo +- eps add another 2^-51 |rh| + 2^-66.  The bound is
 * computed from rh, early and in parallel with the polynomial: EXP_EPS.
 * hi = th is in [1, 2).
 */
#define EXP_EPS(rh) (0x1p-49 * fabs(rh) + 0x1p-63)
#define EXP_SHIFT 0x1.8p52

static always_inline int exp_fast_core_f(int F, double xh, double xl, double *hi, double *lo, double *eps)
{
	double kd = FMADD(F, xh, __exp128_inv, EXP_SHIFT);
	int k = (int)(int32_t)asuint64(kd);
	kd -= EXP_SHIFT;
	double rh = FMADD(F, -kd, __exp128_ln2[0], xh);      /* exact either way */
	double rl = FMADD(F, -kd, __exp128_ln2[1], xl);
	double r = rh + rl;
	double q = FMADD(F, r * r, poly_f(F, __exp128_poly, 4, r), rl);
	int j = k & 127;
	double th = __exp128_tab[2 * j], tl = __exp128_tab[2 * j + 1];
	double big = th * rh;                       /* parallel to the polynomial */
	double tail = FMADD(F, tl, r, tl);
	*hi = th;
	*lo = FMADD(F, th, q, tail) + big;
	*eps = EXP_EPS(rh);
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
static always_inline int log_fast_core_f(int F, double x, double *mh, double *ml, double *r2p)
{
	uint64_t u = asuint64(x);
	uint64_t tmp = u - 0x3fe8000000000000ULL;
	int k = (int)((int64_t)tmp >> 52);
	int i = (int)(tmp >> 45) & 127;
	double m = asdouble(u - ((uint64_t)(int64_t)k << 52));
	double c = __log_inv[i];
	dd_t p = two_prod_f(F, m, c);
	double rh = p.hi - 1.0, rl = p.lo;
	double r = rh + rl;
	double r2 = r * r;
	dd_t h = fast_two_sum(__log_logc[2 * i], rh);
	*mh = h.hi;
	*ml = h.lo + FMADD(F, r2, FMADD(F, r, poly_f(F, __log1p_poly, 7, r), -0.5), __log_logc[2 * i + 1] + rl);
	*r2p = r2;
	return k;
}

static always_inline int log_fast_core(double x, double *mh, double *ml, double *r2p)
{
	return log_fast_core_f(0, x, mh, ml, r2p);
}

#define LOG_ERR(r2) (0x1p-50 * (r2) + 0x1p-80)

/* x mod pi/2 with the medium-range (|x| < 2^20 pi/2) reduction of
 * rem_pio2.c inlined; larger arguments call __rem_pio2. */
static always_inline int rem_pio2_inline(double x, dd_t *r)
{
	if (likely(fabs(x) < 0x1.921fb54442d18p20)) {
		double z = x * __two_over_pi;
		int n = iround(z);
		double nd = (double)n;
		double y1 = x - nd * __pio2_split[0];
		dd_t a = two_sum(y1, -nd * __pio2_split[1]);
		dd_t s = two_sum(a.hi, -nd * __pio2_split[2]);
		s.lo += a.lo - nd * __pio2_split[3];
		*r = fast_two_sum(s.hi, s.lo);
		return n;
	}
	return __rem_pio2(x, r);
}

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
static always_inline dd_t log_acc_core_f(int F, double x, double *err)
{
	uint64_t u = asuint64(x);
	uint64_t tmp = u - 0x3fe8000000000000ULL;
	int k = (int)((int64_t)tmp >> 52);
	int i = (int)(tmp >> 45) & 127;
	double m = asdouble(u - ((uint64_t)(int64_t)k << 52));
	double c = __log_inv[i];
	dd_t p = two_prod_f(F, m, c);
	double rh = p.hi - 1.0, rl = p.lo;
	double r = rh + rl;
	dd_t sq = two_prod_f(F, rh, rh);
	double kd = (double)k;
	dd_t a = fast_two_sum(__log_logc[2 * i], rh);
	dd_t b = two_sum(a.hi, -0.5 * sq.hi);
	dd_t h = two_sum(kd * __log_ln2[0], b.hi);
	double t1 = kd * __log_ln2[1], t2 = __log_logc[2 * i + 1];
	double t3 = rl - rh * rl - 0.5 * sq.lo;
	double t4 = r * r * r * poly_f(F, __log1p_poly, 7, r);
	double lo = ((a.lo + b.lo) + (h.lo + t1)) + ((t2 + t3) + t4);
	double sabs = fabs(a.lo) + fabs(b.lo) + fabs(h.lo) + fabs(t1) + fabs(t2) + fabs(rl) + 0x1p-7 * fabs(rl)
	            + fabs(sq.lo) + fabs(t4);
	*err = 0x1p-48 * sabs + 0x1p-100 * fabs(h.hi);
	return (dd_t){ h.hi, lo };
}

static always_inline dd_t log_acc_core(double x, double *err)
{
	return log_acc_core_f(0, x, err);
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
