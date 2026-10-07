/*
 * lib-spfxd — atan, atan2, asin, acos.
 *
 * atan(v) for 0 <= v <= 1: with c = k/16 nearest to v,
 *     atan(v) = atan(c) + atan(u),  u = (v - c) / (1 + v c),  |u| <= 1/32
 * where atan(c) is a dd table entry and atan(u) = u + u^3 P(u^2).  The
 * numerator is exact and the quotient is formed in dd, so the result is a
 * dd good to ~2^-100.  v > 1 goes through atan(v) = pi/2 - atan(1/v).
 * asin and acos are expressed through atan with dd intermediates, so they
 * keep full accuracy right up to |x| = 1.
 */
#include <math.h>
#include "fp.h"

static dd_t atan_unit(dd_t v)
{
	int k = (int)(v.hi * 16.0 + 0.5);
	double c = k * 0.0625;
	dd_t num = two_sum(v.hi - c, v.lo);           /* v.hi - c is exact */
	dd_t den = two_prod(v.hi, c);
	den.lo += v.lo * c;
	den = dd_add_d(den, 1.0);
	dd_t u = dd_div(num, den);
	double z = u.hi * u.hi;
	dd_t a = fast_two_sum(u.hi, u.lo + u.hi * z * poly(__atan_poly, 5, z));
	return dd_add((dd_t){ __atan_tab[2 * k], __atan_tab[2 * k + 1] }, a);
}

dd_t __atan_dd(dd_t v)
{
	int neg = v.hi < 0;
	if (neg) v = dd_neg(v);
	dd_t r;
	if (v.hi > 1.0) {
		dd_t inv = dd_div((dd_t){ 1.0, 0.0 }, v);
		r = dd_add(dd_c(__pio2_dd), dd_neg(atan_unit(inv)));
	} else {
		r = atan_unit(v);
	}
	return neg ? dd_neg(r) : r;
}

/*
 * Fast path for atan(x), 2^-27 <= |x| < 2^54, without branches on the
 * argument.  With v = |x| and (p, q) = (v, 1) for v <= 1, (1, v) above:
 *     c = round(16 p/q)/16,  u = (p - c q)/(q + c p),  |u| <= 1/32 + tiny
 *     atan(v) = atan(c) + atan(u)            (v <= 1)
 *             = pi/2 - atan(c) - atan(u)     (v > 1, since p/q = 1/v)
 * p - c q is formed exactly (p - (cq)hi is exact by Sterbenz's lemma:
 * either v - c with |v - c| <= 1/32, or 1 - c v with c v in [1/2, 2]),
 * the denominator as a dd, and the quotient with one correction step, so
 * u = uh + ul is good to ~2^-100 relative.  atan(u) = u + u^3 P(u^2).
 * The rounded terms are u^3 P (|.| <= 2^-15/3, two roundings) and sums of
 * dd tails at the 2^-53 |result| level, so the error is below
 * 2^-49 |uh|^3 + 2^-98 |result|.
 */
static always_inline double sel(uint64_t m, double a, double b)
{
	return asdouble((asuint64(b) & m) | (asuint64(a) & ~m));
}

static always_inline int atan_fast(double x, double *res)
{
	double v = fabs(x);
	uint64_t big = -(uint64_t)(v > 1.0);          /* all ones when v > 1 */
	double p = sel(big, v, 1.0), q = sel(big, 1.0, v);
	int k = iround(16.0 * (p / q));
	double c = (double)k * 0.0625;
	dd_t cq = two_prod(c, q), cp = two_prod(c, p);
	dd_t num = { p - cq.hi, -cq.lo };
	dd_t den = two_sum(q, cp.hi);
	den.lo += cp.lo;
	/* u = num / den: first approximation, then the exact residual */
	double uh = num.hi / den.hi;
	dd_t t = two_prod(uh, den.hi);
	double rr = ((num.hi - t.hi) - t.lo) + num.lo - uh * den.lo;
	double ul = rr / den.hi;
	double z = uh * uh;
	double u3 = uh * z * poly(__atan_poly, 5, z);
	dd_t s = fast_two_sum(__atan_tab[2 * k], uh);
	double lo = s.lo + (__atan_tab[2 * k + 1] + ul + u3);
	/* v > 1: pi/2 - (s.hi + lo); the selects keep this branch-free */
	double sg = sel(big, 1.0, -1.0);
	dd_t f = two_sum(sel(big, 0.0, __pio2_dd[0]), sg * s.hi);
	double flo = f.lo + (sel(big, 0.0, __pio2_dd[1]) + sg * lo);
	double eps = 0x1p-49 * fabs(uh * z) + 0x1p-98 * fabs(f.hi);
	/* apply the sign before rounding, so directed modes round the
	 * signed value (negation is exact) */
	uint64_t sx = asuint64(x) & 0x8000000000000000ULL;
	double fh = asdouble(asuint64(f.hi) ^ sx), fl = asdouble(asuint64(flo) ^ sx);
	double a = fh + (fl - eps), b = fh + (fl + eps);
	*res = a;
	return asuint64(a) == asuint64(b);
}

double atan(double x)
{
	uint32_t top = TOP12(x) & 0x7ff;
	if (likely(top - 0x3e4 < 0x435 - 0x3e4)) {   /* 2^-27 <= |x| < 2^54 */
		double r;
		if (likely(atan_fast(x, &r))) return r;
	}
	if (top < 0x3e4) {                       /* |x| < 2^-27 */
		if (top < 0x010) fp_force_eval(x * x);
		/* the true value lies strictly beyond/before x by far less than
		 * half an ulp: nudge so that directed rounding goes the right way
		 * (to nearest this is x itself); zero keeps its sign */
		return x == 0 ? x : x - x * 0x1p-60;
	}
	if (top >= 0x435) {                      /* |x| >= 2^54, inf, nan */
		if (__builtin_isnan(x)) return x + x;
		/* atan x = sgn(x) pi/2 - 1/x + O(x^-3); the signed sum is
		 * rounded once, which matters in directed rounding modes */
		double sg = x < 0 ? -1.0 : 1.0;
		return sg * __pio2_dd[0] + (sg * __pio2_dd[1] - 1.0 / x);
	}
	dd_t r = __atan_dd(dd_from(x));
	return r.hi + r.lo;
}

double atan2(double y, double x)
{
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	int sy = __builtin_signbit(y), sx = __builtin_signbit(x);
	dd_t pi = dd_c(__pi_dd), pio2 = dd_c(__pio2_dd);
	double r;

	if (y == 0) {
		if (!sx) return y;                   /* +-0 */
		r = pi.hi + pi.lo;
		return sy ? -r : r;
	}
	if (x == 0) {
		r = pio2.hi + pio2.lo;
		return sy ? -r : r;
	}
	if (__builtin_isinf(x)) {
		if (__builtin_isinf(y)) {
			dd_t q = sx ? dd_mul_d(pi, 0.75) : dd_mul_d(pi, 0.25);
			r = q.hi + q.lo;
		} else {
			r = sx ? pi.hi + pi.lo : 0.0;
		}
		return sy ? -r : r;
	}
	if (__builtin_isinf(y)) {
		r = pio2.hi + pio2.lo;
		return sy ? -r : r;
	}

	double ax = fabs(x), ay = fabs(y);
	int ex = EXP_BITS(ax), ey = EXP_BITS(ay);
	/* effective biased exponents (subnormals below 1) */
	if (!ex) ex = -__builtin_clzll(asuint64(ax) << 12);
	if (!ey) ey = -__builtin_clzll(asuint64(ay) << 12);
	dd_t a;
	if (ey - ex > 60) {
		/* |y/x| > 2^60: pi/2 - x/|y| */
		a = dd_add_d(pio2, -x / ay);
		r = a.hi + a.lo;
		return sy ? -r : r;
	}
	if (ex - ey > 60) {
		if (!sx) {
			/* tiny quotient; may underflow */
			r = __math_check_uflow(ay / ax);
			return sy ? -r : r;
		}
		a = dd_add_d(pi, -(ay / ax));
		r = a.hi + a.lo;
		return sy ? -r : r;
	}
	/* bring both into a range where the dd operations cannot overflow
	 * or lose bits to underflow */
	if (ex > 0x3ff + 500 || ey > 0x3ff + 500) { ax *= 0x1p-600; ay *= 0x1p-600; }
	else if (ex < 0x3ff - 500 || ey < 0x3ff - 500) { ax *= 0x1p600; ay *= 0x1p600; }
	a = __atan_dd(dd_div(dd_from(ay), dd_from(ax)));
	if (sx) a = dd_add(pi, dd_neg(a));
	r = a.hi + a.lo;
	return sy ? -r : r;
}

double asin(double x)
{
	double a = fabs(x);
	if (!__builtin_islessequal(a, 1.0)) return __builtin_isnan(x) ? x + x : __math_invalid(x);
	uint32_t top = TOP12(x) & 0x7ff;
	if (top < 0x3e4) {
		if (top < 0x010) fp_force_eval(x * x);
		/* the true value lies strictly beyond/before x by far less than
		 * half an ulp: nudge so that directed rounding goes the right way
		 * (to nearest this is x itself); zero keeps its sign */
		return x == 0 ? x : x + x * 0x1p-60;
	}
	dd_t r;
	if (a == 1.0) {
		r = dd_c(__pio2_dd);
	} else {
		/* 1 - a^2 as dd */
		dd_t w;
		if (a >= 0.5) {
			w = dd_mul_d(two_sum(1.0, a), 1.0 - a);      /* 1 - a exact */
		} else {
			dd_t p = two_prod(a, a);
			w = two_sum(1.0, -p.hi);
			w.lo -= p.lo;
			w = fast_two_sum(w.hi, w.lo);
		}
		dd_t s = dd_sqrt(w);
		r = __atan_dd(dd_div(dd_from(a), s));
	}
	double res = r.hi + r.lo;
	return x < 0 ? -res : res;
}

double acos(double x)
{
	double a = fabs(x);
	if (!__builtin_islessequal(a, 1.0)) return __builtin_isnan(x) ? x + x : __math_invalid(x);
	if (x == 1.0) return 0.0;
	if (x == -1.0) return __pi_dd[0] + fp_barrier(__pi_dd[1]);
	if (a < 0x1p-57) return __pio2_dd[0] + fp_barrier(__pio2_dd[1]);
	/* acos(x) = 2 atan(sqrt((1 - x) / (1 + x))) */
	dd_t q = dd_div(two_sum(1.0, -x), two_sum(1.0, x));
	dd_t r = __atan_dd(dd_sqrt(q));
	return 2.0 * r.hi + 2.0 * r.lo;
}
