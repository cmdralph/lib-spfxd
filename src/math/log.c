/*
 * lib-spfxd — log, log2, log10, log1p and the shared dd kernel.
 *
 * x = 2^k m with m in [0.75, 1.5), found by subtracting the bit pattern of
 * 0.75 from that of x.  The next 7 bits pick an interval with an
 * approximate inverse c, and
 *     log(x) = k ln2 - log(c) + log1p(m c - 1).
 * m c - 1 is computed exactly as a dd (two_prod, then a Sterbenz
 * subtraction); log1p(r) = r - r^2/2 + r^3 P(r) with |r| <= 2^-7 and P of
 * degree 7.  The intervals next to 1 use c = 1, so log(x) for x near 1 is
 * computed without any cancellation.  The dd result has relative error
 * below 2^-68, which pow relies on.
 */
#include <errno.h>
#include <math.h>
#include "fp.h"
#include "fastpath.h"

/* log(m) for the normalized significand of x (as dd) and the exponent k. */
static dd_t log_parts(double x, int *kp)
{
	uint64_t u = asuint64(x);
	int kadj = 0;
	if (u < (1ULL << 52)) {             /* subnormal */
		u = asuint64(x * 0x1p52);
		kadj = -52;
	}
	uint64_t tmp = u - 0x3fe8000000000000ULL;
	int k = (int)((int64_t)tmp >> 52);
	int i = (int)(tmp >> 45) & 127;
	double m = asdouble(u - ((uint64_t)(int64_t)k << 52));
	*kp = k + kadj;

	double c = __log_inv[i];
	dd_t r;
	if (c == 1.0) {
		r = (dd_t){ m - 1.0, 0.0 };
	} else {
		dd_t p = two_prod(m, c);
		r = two_sum(p.hi - 1.0, p.lo);
	}
	/* log1p(r) = r - r^2/2 + r^3 P(r) */
	dd_t h = two_prod(r.hi, r.hi);
	h.hi *= 0.5;
	h.lo *= 0.5;
	double rest = r.lo - h.lo - r.hi * r.lo + h.hi * 2.0 * r.hi * poly(__log1p_poly, 7, r.hi);
	dd_t s = two_sum(r.hi, -h.hi);
	s.lo += rest;
	s = fast_two_sum(s.hi, s.lo);
	return dd_add(s, (dd_t){ __log_logc[2 * i], __log_logc[2 * i + 1] });
}

dd_t __log_dd(double x)
{
	int k;
	dd_t l = log_parts(x, &k);
	if (!k) return l;
	double kd = (double)k;
	dd_t t = { kd * __log_ln2[0], kd * __log_ln2[1] };   /* first product exact */
	t = fast_two_sum(t.hi, t.lo);
	return dd_add(t, l);
}

/* Shared argument screening: returns 1 and sets *res for special x. */
static int log_special(double x, double *res)
{
	uint64_t u = asuint64(x);
	if (likely(u - 0x0010000000000000ULL < 0x7ff0000000000000ULL - 0x0010000000000000ULL))
		return 0;                       /* positive normal */
	if (!(u << 1)) { *res = __math_divzero(1); return 1; }
	if (u == 0x7ff0000000000000ULL) { *res = x; return 1; }
	if ((u >> 63) || (u & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL) {
		*res = __builtin_isnan(x) ? x + x : __math_invalid(x);
		return 1;
	}
	return 0;                               /* positive subnormal */
}

double log(double x)
{
	double r;
	uint64_t u = asuint64(x);
	if (likely(u - 0x0010000000000000ULL < 0x7ff0000000000000ULL - 0x0010000000000000ULL)) {
		/* fast path for positive normal x:
		 * log x = k ln2 + mh + ml, k ln2hi exact (42-bit constant) */
		double mh, ml, r2;
		int k = log_fast_core(x, &mh, &ml, &r2);
		double kd = (double)k;
		dd_t h = two_sum(kd * __log_ln2[0], mh);
		double lo = h.lo + (kd * __log_ln2[1] + ml);
		/* two_sum is exact; k ln2lo errs by < 2^-85 */
		double eps = LOG_ERR(r2) + 0x1p-84;
		double a;
		if (likely(ziv_round(h.hi, lo, eps, &a))) return a;
	}
	if (log_special(x, &r)) return r;
	dd_t l = __log_dd(x);
	return l.hi + l.lo;
}

/* (mh + ml) * C for a dd constant C, as hi + lo (error < 2^-52 r2 more
 * than C times that of ml; see fastpath.h). */
static always_inline dd_t log_scale(double mh, double ml, const double *c)
{
	dd_t p = two_prod(mh, c[0]);
	p.lo += mh * c[1] + ml * c[0];
	return p;
}

#define LOGN_ERR(r2) (0x1p-49 * (r2) + 0x1p-78)

double log2(double x)
{
	double r;
	uint64_t u = asuint64(x);
	if (likely(u - 0x0010000000000000ULL < 0x7ff0000000000000ULL - 0x0010000000000000ULL)) {
		double mh, ml, r2;
		int k = log_fast_core(x, &mh, &ml, &r2);
		dd_t p = log_scale(mh, ml, __inv_ln2);
		dd_t h = two_sum((double)k, p.hi);
		double lo = h.lo + p.lo;
		double eps = LOGN_ERR(r2);
		double a;
		if (likely(ziv_round(h.hi, lo, eps, &a))) return a;
	}
	if (log_special(x, &r)) return r;
	int k;
	dd_t l = log_parts(x, &k);
	l = dd_mul(l, dd_c(__inv_ln2));
	if (k) l = dd_add_d(l, (double)k);
	return l.hi + l.lo;
}

double log10(double x)
{
	double r;
	uint64_t u = asuint64(x);
	if (likely(u - 0x0010000000000000ULL < 0x7ff0000000000000ULL - 0x0010000000000000ULL)) {
		double mh, ml, r2;
		int k = log_fast_core(x, &mh, &ml, &r2);
		dd_t p = log_scale(mh, ml, __inv_ln10);
		/* k log10(2) as an exact product plus the constant's tail */
		dd_t t = two_prod((double)k, __log10_2[0]);
		t.lo += (double)k * __log10_2[1];
		dd_t h = two_sum(t.hi, p.hi);
		double lo = h.lo + (t.lo + p.lo);
		double eps = LOGN_ERR(r2) + 0x1p-100 * fabs(h.hi);
		double a;
		if (likely(ziv_round(h.hi, lo, eps, &a))) return a;
	}
	if (log_special(x, &r)) return r;
	int k;
	dd_t l = log_parts(x, &k);
	l = dd_mul(l, dd_c(__inv_ln10));
	if (k) l = dd_add(dd_mul_d(dd_c(__log10_2), (double)k), l);
	return l.hi + l.lo;
}

double log1p(double x)
{
	uint64_t u = asuint64(x);
	uint32_t top = (uint32_t)(u >> 52) & 0x7ff;
	if (unlikely(!__builtin_isgreater(x, -1.0))) {
		if (x == -1.0) return __math_divzero(1);
		return __builtin_isnan(x) ? x + x : __math_invalid(x);
	}
	if (top >= 0x7ff) return x;             /* +inf */
	if (top < 0x3c9) {                      /* |x| < 2^-54 */
		/* the true value lies strictly beyond/before x by far less than
		 * half an ulp: nudge so that directed rounding goes the right way
		 * (to nearest this is x itself); zero keeps its sign */
		return x == 0 ? x : x - fabs(x) * 0x1p-60;
	}
	/* 1 + x = s.hi + s.lo exactly; log(1+x) = log(s.hi) + s.lo / s.hi */
	dd_t s = two_sum(1.0, x);
	dd_t l = __log_dd(s.hi);
	if (s.lo != 0) l = dd_add_d(l, s.lo / s.hi);
	return l.hi + l.lo;
}
