/*
 * lib-spfxd — fast paths for expf, exp2f, logf, log2f, log10f, powf,
 * sinf, cosf, sincosf and tanf.
 *
 * Each computes a double approximation with relative error below 2^-49
 * using the cores in fastpath.h (whose own errors are near 2^-58) and
 * plain double polynomials, and rounds it to float when float_round_ok
 * says the rounding is certain.  Otherwise — about one argument in 2^21,
 * plus exact cases such as logf(1) or exp2f(3), and any result outside
 * the normal float range — the function returns the double
 * implementation's result rounded once, exactly as float.c does for the
 * other functions.  The fast path therefore never changes a result.
 */
#include <math.h>
#include "fp.h"
#include "fastpath.h"

/* ---------------------------------------------------------------- exp */

float expf(float x)
{
	/* e^x is a normal float for x in (-87.33, 88.72) */
	if (likely(x > -87.3f && x < 88.7f)) {
		double hi, lo;
		int s = exp_fast_core(x, 0.0, &hi, &lo);
		double y = scale_normal(hi + lo, s);
		if (likely(float_round_ok(y))) return (float)y;
	}
	return __math_narrowf(exp(x));
}

float exp2f(float x)
{
	if (likely(x > -126.0f && x < 127.9f)) {
		/* x ln2 as an exact product plus the constant's tail */
		dd_t t = two_prod(x, __ln2[0]);
		t.lo += x * __ln2[1];
		double hi, lo;
		int s = exp_fast_core(t.hi, t.lo, &hi, &lo);
		double y = scale_normal(hi + lo, s);
		if (likely(float_round_ok(y))) return (float)y;
	}
	return __math_narrowf(exp2(x));
}

/* ---------------------------------------------------------------- log */

/* log of a positive finite float as a dd (k ln2 + log m); every float is
 * a normal double, so log_fast_core applies directly. */
static always_inline dd_t logf_dd(double x)
{
	double mh, ml, r2;
	int k = log_fast_core(x, &mh, &ml, &r2);
	double kd = (double)k;
	dd_t h = two_sum(kd * __log_ln2[0], mh);
	h.lo += kd * __log_ln2[1] + ml;
	return h;
}

static always_inline int pos_finite(float x)
{
	return asuint(x) - 1 < 0x7f800000u - 1;     /* 0 < x < inf */
}

float logf(float x)
{
	if (likely(pos_finite(x))) {
		dd_t l = logf_dd(x);
		double y = l.hi + l.lo;
		if (likely(float_round_ok(y))) return (float)y;
	}
	return (float)log(x);
}

static always_inline float logf_scaled(float x, const double *c, double (*slow)(double))
{
	if (likely(pos_finite(x))) {
		double mh, ml, r2;
		int k = log_fast_core(x, &mh, &ml, &r2);
		double y = (mh * c[0] + (ml * c[0] + mh * c[1])) + (double)k * (c == __inv_ln2 ? 1.0 : __log10_2[0]);
		if (likely(float_round_ok(y))) return (float)y;
	}
	return (float)slow(x);
}

float log2f(float x)
{
	return logf_scaled(x, __inv_ln2, log2);
}

float log10f(float x)
{
	return logf_scaled(x, __inv_ln10, log10);
}

/* ---------------------------------------------------------------- pow */

float powf(float x, float y)
{
	if (likely(pos_finite(x) && __builtin_isfinite(y))) {
		/* t = y log x as a dd: the exponential magnifies an absolute
		 * error in t into the same relative error of the result */
		dd_t l = logf_dd(x);
		dd_t t = two_prod(y, l.hi);
		t.lo += y * l.lo;
		if (likely(t.hi > -87.3 && t.hi < 88.7)) {
			double hi, lo;
			int s = exp_fast_core(t.hi, t.lo, &hi, &lo);
			double r = scale_normal(hi + lo, s);
			if (likely(float_round_ok(r))) return (float)r;
		}
	}
	return __math_narrowf(pow(x, y));
}

/* ---------------------------------------------------------------- trig */

/* sin and cos of the reduced argument r (|r| <= pi/4 + tiny); relative
 * errors near 2^-52. */
static always_inline double sin_r(double r, double z)
{
	return r + r * z * poly(__sin_poly, 6, z);
}

static always_inline double cos_r(double z)
{
	return 1.0 - 0.5 * z + z * z * poly(__cos_poly, 6, z);
}

static always_inline double pick(int odd, double a, double b)
{
	uint64_t m = -(uint64_t)(odd & 1);
	return asdouble((asuint64(b) & m) | (asuint64(a) & ~m));
}

static always_inline double negate_if(int c, double v)
{
	return asdouble(asuint64(v) ^ ((uint64_t)(c != 0) << 63));
}

/* The fast path covers 2^-60 <= |x| < 2^20 (results are then normal
 * floats; beyond 2^20 the medium reduction no longer applies). */
static always_inline int trigf_range(float x)
{
	uint32_t a = asuint(x) & 0x7fffffffu;
	return a - 0x21800000u < 0x49800000u - 0x21800000u;
}

float sinf(float x)
{
	if (likely(trigf_range(x))) {
		dd_t r;
		int n = __rem_pio2(x, &r);
		double z = r.hi * r.hi;
		double v = negate_if(n & 2, pick(n, sin_r(r.hi, z), cos_r(z)));
		if (likely(float_round_ok(v))) return (float)v;
	}
	return __math_narrowf(sin(x));
}

float cosf(float x)
{
	if (likely(trigf_range(x))) {
		dd_t r;
		int n = __rem_pio2(x, &r) + 1;          /* cos x = sin(x + pi/2) */
		double z = r.hi * r.hi;
		double v = negate_if(n & 2, pick(n, sin_r(r.hi, z), cos_r(z)));
		if (likely(float_round_ok(v))) return (float)v;
	}
	return (float)cos(x);
}

void sincosf(float x, float *sp, float *cp)
{
	if (likely(trigf_range(x))) {
		dd_t r;
		int n = __rem_pio2(x, &r);
		double z = r.hi * r.hi;
		double s = sin_r(r.hi, z), c = cos_r(z);
		double sv = negate_if(n & 2, pick(n, s, c));
		double cv = negate_if((n + 1) & 2, pick(n, c, s));
		if (likely(float_round_ok(sv) && float_round_ok(cv))) {
			*sp = (float)sv;
			*cp = (float)cv;
			return;
		}
	}
	double sd, cd;
	sincos(x, &sd, &cd);
	*sp = __math_narrowf(sd);
	*cp = (float)cd;
}

float tanf(float x)
{
	if (likely(trigf_range(x))) {
		dd_t r;
		int n = __rem_pio2(x, &r);
		double z = r.hi * r.hi;
		double s = sin_r(r.hi, z), c = cos_r(z);
		/* s/c, or -c/s in odd quadrants: relative error ~3 * 2^-53 */
		double v = negate_if(n & 1, pick(n, s, c) / pick(n, c, s));
		if (likely(fabs(v) < 0x1p127 && float_round_ok(v))) return (float)v;
	}
	return __math_narrowf(tan(x));
}
