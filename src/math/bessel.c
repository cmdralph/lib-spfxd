/*
 * lib-spfxd — Bessel functions of the first and second kind:
 * j0 j1 jn y0 y1 yn (and float versions).
 *
 *   |x| <= 20  ascending power series summed in double-double; the
 *              largest term is below 2^24, so ~2^-80 absolute accuracy
 *              remains, enough for full relative accuracy except within
 *              ~2^-28 of a zero
 *   |x| > 20   Hankel asymptotic expansion (P, Q series in 1/x, smallest
 *              term ~e^-2x < 2^-57) with sin/cos of the exactly reduced x
 *   jn, yn     Hankel expansion of order n when |x| > n^2/2 + 25;
 *              otherwise jn uses the forward recurrence when n < |x| and
 *              Miller's backward recurrence normalized by J0 or J1 when
 *              not, and yn the (always stable) forward recurrence, all
 *              in double-double
 *
 * Known limitation: for |x| > 20 accuracy is absolute (~2^-53 times the
 * envelope sqrt(2/(pi x))), so relative accuracy degrades near zeros.
 */
#include <errno.h>
#include <math.h>
#include "fp.h"

#define SERIES_MAX 20.0

/* sum_{k>=0} (-q)^k / (k! (k+nu)!) for nu = 0, 1 with q = x^2/4 (dd).
 * If h is non-null also sums the harmonic-weighted series needed by Y:
 *   nu = 0: sum_{k>=1} (-1)^k H_k q^k / (k!)^2
 *   nu = 1: sum_{k>=0} (-1)^k (H_k + H_{k+1}) q^k / (k! (k+1)!)       */
static dd_t series(dd_t q, int nu, dd_t *h)
{
	dd_t term = dd_from(1.0);               /* q^0 / (0! nu!) = 1 */
	dd_t sum = term;
	dd_t hk = dd_from(0.0);                 /* H_k */
	dd_t hsum = dd_from(0.0);
	if (h && nu) hsum = dd_from(1.0);        /* k = 0: H_0 + H_1 = 1 */
	double tmax = 1.0;
	for (int k = 1; k < 200; k++) {
		term = dd_div(dd_mul(term, q), dd_from(-(double)k * (k + nu)));
		sum = dd_add(sum, term);
		if (h) {
			hk = dd_add(hk, dd_div(dd_from(1.0), dd_from((double)k)));
			dd_t w = hk;
			if (nu) w = dd_add(hk, dd_add(hk, dd_div(dd_from(1.0), dd_from((double)(k + 1)))));
			hsum = dd_add(hsum, dd_mul(w, term));
		}
		double at = fabs(term.hi);
		if (at > tmax) tmax = at;
		if (at < tmax * 0x1p-110 && k > 2) break;
	}
	if (h) *h = hsum;
	return sum;
}

/* Hankel asymptotic P and Q for order nu (0 or 1), x > 20 */
static void hankel(double x, int nu, double *p, double *q)
{
	double mu = 4.0 * nu * nu;
	double a = 1.0, inv = 1.0 / x, pw = 1.0;
	double ps = 1.0, qs = 0.0, prev = 1e300;
	for (int k = 1; k < 60; k++) {
		a *= (mu - (2.0 * k - 1) * (2.0 * k - 1)) / (8.0 * k);
		pw *= inv;
		double t = a * pw;
		if (fabs(t) > prev) break;          /* asymptotic series diverging */
		prev = fabs(t);
		/* k even -> P with sign (-1)^(k/2); k odd -> Q with (-1)^((k-1)/2) */
		switch (k & 3) {
		case 0: ps += t; break;
		case 1: qs += t; break;
		case 2: ps -= t; break;
		case 3: qs -= t; break;
		}
		if (fabs(t) < 0x1p-60) break;
	}
	*p = ps;
	*q = qs;
}

/* sqrt(2/(pi x)) (P cos chi - Q sin chi) or (P sin chi + Q cos chi) with
 * chi = x - pi/4 - nu pi/2.  chi is reduced exactly: x = n pi/2 + r, so
 * chi = (n - nu) pi/2 + (r - pi/4) with the dd offset brought back into
 * [-pi/4, pi/4].  Near a zero cos chi (or sin chi) is then accurate to
 * full relative precision and the only cancellation left is between two
 * terms of size |Q| ~ 1/(8x). */
static double asym(double x, int nu, int second)
{
	double p, q;
	hankel(x, nu, &p, &q);
	dd_t r;
	int m = __rem_pio2(x, &r) - nu;
	dd_t pio4 = { 0.5 * __pio2_dd[0], 0.5 * __pio2_dd[1] };
	dd_t t = dd_add(r, dd_neg(pio4));
	if (t.hi < -pio4.hi) {
		t = dd_add(t, dd_c(__pio2_dd));
		m--;
	} else if (t.hi > pio4.hi) {
		t = dd_add(t, dd_neg(dd_c(__pio2_dd)));
		m++;
	}
	double st = __sin_kernel(t), ct = __cos_kernel(t), sc, cc;
	switch (m & 3) {
	case 0: sc = st; cc = ct; break;
	case 1: sc = ct; cc = -st; break;
	case 2: sc = -st; cc = -ct; break;
	default: sc = -ct; cc = st; break;
	}
	double v = second ? p * sc + q * cc : p * cc - q * sc;
	return v * __sqrt_2_over_pi[0] / sqrt(x);
}

/* (2/pi)(log(x/2) + gamma) as dd */
static dd_t ylog(double x)
{
	dd_t l = dd_add(__log_dd(x), dd_neg(dd_c(__ln2)));
	l = dd_add(l, dd_c(__euler_dd));
	return dd_mul(l, dd_c(__two_over_pi_dd));
}

/* J_nu(a) for nu = 0, 1 and finite a > 0, as dd */
static dd_t jcore(int nu, double a)
{
	if (a > SERIES_MAX) return dd_from(asym(a, nu, 0));
	dd_t q = two_prod(a * 0.5, a * 0.5);
	dd_t s = series(q, nu, 0);
	return nu ? dd_mul_d(s, a * 0.5) : s;
}

/* Y_nu(x) for nu = 0, 1 and finite x > 2^-54, as dd */
static dd_t ycore(int nu, double x)
{
	if (x > SERIES_MAX) return dd_from(asym(x, nu, 1));
	dd_t h;
	double hx = 0.5 * x;
	dd_t q = two_prod(hx, hx);
	if (!nu) {
		/* Y0 = (2/pi)(log(x/2) + gamma) J0 - (2/pi) sum (-1)^k H_k q^k/(k!)^2 */
		dd_t j = series(q, 0, &h);
		return dd_add(dd_mul(ylog(x), j), dd_neg(dd_mul(h, dd_c(__two_over_pi_dd))));
	}
	/* Y1 = (2/pi)(log(x/2)+gamma) J1 - 2/(pi x) - (1/pi) (x/2) hsum */
	dd_t j = dd_mul_d(series(q, 1, &h), hx);
	dd_t r = dd_mul(ylog(x), j);
	r = dd_add(r, dd_neg(dd_div(dd_c(__two_over_pi_dd), dd_from(x))));
	return dd_add(r, dd_neg(dd_mul_d(dd_mul(h, dd_c(__inv_pi_dd)), hx)));
}

double j0(double x)
{
	double a = fabs(x);
	if (__builtin_isnan(x)) return x + x;
	if (__builtin_isinf(x)) return 0.0;
	if (a < 0x1p-27) return 1.0 - 0.25 * x * x;
	dd_t r = jcore(0, a);
	return r.hi + r.lo;
}

double j1(double x)
{
	double a = fabs(x);
	if (__builtin_isnan(x)) return x + x;
	if (__builtin_isinf(x)) return 0.0;
	if (a < 0x1p-27) return 0.5 * x;
	dd_t r = jcore(1, a);
	double v = r.hi + r.lo;
	return x < 0 ? -v : v;
}

double y0(double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (x < 0) return __math_invalid(x);
	if (x == 0) return __math_divzero(1);
	if (__builtin_isinf(x)) return 0.0;
	if (x < 0x1p-27) {
		dd_t r = ylog(x);
		return r.hi + r.lo;
	}
	dd_t r = ycore(0, x);
	return r.hi + r.lo;
}

double y1(double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (x < 0) return __math_invalid(x);
	if (x == 0) return __math_divzero(1);
	if (__builtin_isinf(x)) return 0.0;
	if (x < 0x1p-54) return __math_check_oflow(-__two_over_pi_dd[0] / x);   /* -2/(pi x) */
	dd_t r = ycore(1, x);
	return r.hi + r.lo;
}

double jn(int n, double x)
{
	if (__builtin_isnan(x)) return x + x;
	int sign = 0;
	if (n < 0) {
		/* J_{-n} = (-1)^n J_n; avoid overflow of -INT_MIN */
		if (n == -2147483647 - 1) return jn(n + 1, x) * 0.0;
		n = -n;
		sign = n & 1;
	}
	if (x < 0) {
		x = -x;
		sign ^= n & 1;
	}
	double r;
	if (n == 0) r = j0(x);
	else if (n == 1) r = j1(x);
	else if (x == 0 || __builtin_isinf(x)) r = 0.0;
	else if (x > SERIES_MAX && x > 0.5 * n * (double)n + 25.0) r = asym(x, n, 0);
	else if ((double)n < x) {
		/* forward recurrence J_{k+1} = (2k/x) J_k - J_{k-1}, stable for k < x */
		dd_t a = jcore(0, x), b = jcore(1, x);
		for (int k = 1; k < n; k++) {
			dd_t c = dd_add(dd_mul(dd_div(dd_from(2.0 * k), dd_from(x)), b), dd_neg(a));
			a = b;
			b = c;
		}
		r = b.hi + b.lo;
	} else if (x * x < 1e-8 * (n + 1)) {
		/* leading series term (x/2)^n / n!, computed without overflow */
		double t = 1.0;
		for (int k = 1; k <= n && t != 0; k++) t *= (0.5 * x) / k;
		r = t;
	} else {
		/* Miller: backward from N with J_{N+1} = 0, J_N = tiny, in dd */
		double big = n > x ? (double)n : x;
		int N = (int)(big + 30.0 + 2.0 * sqrt(40.0 * big));
		dd_t jp = dd_from(0.0), jc = dd_from(0x1p-500), jn_val = dd_from(0.0);
		dd_t j0v = jn_val, j1v = jn_val;
		dd_t ix = dd_div(dd_from(2.0), dd_from(x));
		for (int k = N; k > 0; k--) {
			dd_t jm = dd_add(dd_mul(dd_mul_d(ix, (double)k), jc), dd_neg(jp));
			jp = jc;
			jc = jm;
			if (fabs(jc.hi) > 0x1p500) {
				jc = (dd_t){ jc.hi * 0x1p-1000, jc.lo * 0x1p-1000 };
				jp = (dd_t){ jp.hi * 0x1p-1000, jp.lo * 0x1p-1000 };
				jn_val = (dd_t){ jn_val.hi * 0x1p-1000, jn_val.lo * 0x1p-1000 };
				j1v = (dd_t){ j1v.hi * 0x1p-1000, j1v.lo * 0x1p-1000 };
			}
			if (k - 1 == n) jn_val = jc;
			if (k - 1 == 1) j1v = jc;
			if (k - 1 == 0) j0v = jc;
		}
		dd_t t0 = jcore(0, x), t1 = jcore(1, x);
		dd_t res = fabs(t0.hi) > fabs(t1.hi) ? dd_mul(jn_val, dd_div(t0, j0v))
		                                     : dd_mul(jn_val, dd_div(t1, j1v));
		r = res.hi + res.lo;
	}
	if (sign) r = -r;
	if (r != 0 && fabs(r) < 0x1p-1022) errno = ERANGE;
	return r;
}

double yn(int n, double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (x < 0) return __math_invalid(x);
	if (x == 0) return __math_divzero(1);
	if (__builtin_isinf(x)) return 0.0;
	int sign = 0;
	if (n < 0) {
		if (n == -2147483647 - 1) n++;
		n = -n;
		sign = n & 1;
	}
	double r;
	if (n == 0) r = y0(x);
	else if (n == 1) r = y1(x);
	else if (x > SERIES_MAX && x > 0.5 * n * (double)n + 25.0) r = asym(x, n, 1);
	else if (x < 0x1p-54) {
		r = -__builtin_inf();
		errno = ERANGE;
	} else {
		dd_t a = ycore(0, x), b = ycore(1, x);
		dd_t ix = dd_div(dd_from(2.0), dd_from(x));
		int k = 1;
		for (; k < n && fabs(b.hi) < 0x1p900; k++) {
			dd_t c = dd_add(dd_mul(dd_mul_d(ix, (double)k), b), dd_neg(a));
			a = b;
			b = c;
		}
		/* huge values: dd splitting would overflow, finish in double */
		double ad = a.hi + a.lo, bd = b.hi + b.lo;
		for (; k < n && !__builtin_isinf(bd); k++) {
			double c = (2.0 * k / x) * bd - ad;
			ad = bd;
			bd = c;
		}
		r = bd;
		if (__builtin_isinf(r)) errno = ERANGE;
	}
	return sign ? -r : r;
}

float j0f(float x) { return (float)j0(x); }
float j1f(float x) { return (float)j1(x); }
float jnf(int n, float x) { return __math_narrowf(jn(n, x)); }
float y0f(float x) { return __math_narrowf(y0(x)); }
float y1f(float x) { return __math_narrowf(y1(x)); }
float ynf(int n, float x) { return __math_narrowf(yn(n, x)); }
