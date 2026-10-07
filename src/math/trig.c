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
#include "fastpath.h"

/* Inlinable kernels; the hidden __sin_dd etc. below wrap them for the
 * other files that need them (gamma, sinpi, complex). */
static always_inline dd_t sin_dd(dd_t r)
{
	double z = r.hi * r.hi;
	double t = r.hi * z * poly(__sin_poly, 6, z) + r.lo * (1.0 - 0.5 * z);
	return fast_two_sum(r.hi, t);
}

static always_inline double sin_k(dd_t r)
{
	double z = r.hi * r.hi;
	return r.hi + (r.hi * z * poly(__sin_poly, 6, z) + r.lo * (1.0 - 0.5 * z));
}

static always_inline dd_t cos_dd(dd_t r)
{
	dd_t z = two_prod(r.hi, r.hi);
	double hz = 0.5 * z.hi;
	double w = 1.0 - hz;
	double e = (1.0 - w) - hz;            /* exact rounding error of w */
	double lo = e - 0.5 * z.lo + z.hi * z.hi * poly(__cos_poly, 6, z.hi) - r.hi * r.lo;
	return fast_two_sum(w, lo);
}

static always_inline double cos_k(dd_t r)
{
	dd_t c = cos_dd(r);
	return c.hi + c.lo;
}

dd_t __sin_dd(dd_t r) { return sin_dd(r); }
double __sin_kernel(dd_t r) { return sin_k(r); }
dd_t __cos_dd(dd_t r) { return cos_dd(r); }
double __cos_kernel(dd_t r) { return cos_k(r); }

/* Branch-free quadrant handling: the quadrant of a random argument is
 * unpredictable, so both kernels are evaluated (they are independent and
 * overlap in the pipeline) and the result is selected with bit masks. */
static always_inline double pick(int odd, double a, double b)
{
	uint64_t m = -(uint64_t)(odd & 1);
	return asdouble((asuint64(b) & m) | (asuint64(a) & ~m));
}

static always_inline double negate_if(int c, double v)
{
	return asdouble(asuint64(v) ^ ((uint64_t)(c != 0) << 63));
}

/* sin(n pi/2 + r) */
static always_inline double sin_quadrant(int n, dd_t r)
{
	double s = sin_k(r), c = cos_k(r);
	return negate_if(n & 2, pick(n, s, c));
}

double sin(double x)
{
	uint32_t top = TOP12(x) & 0x7ff;
	if (top < 0x3e5) {                     /* |x| < 2^-26 */
		if (top < 0x010) fp_force_eval(x * x);   /* underflow for subnormal */
		/* the true value lies strictly beyond/before x by far less than
		 * half an ulp: nudge so that directed rounding goes the right way
		 * (to nearest this is x itself); zero keeps its sign */
		return x == 0 ? x : x - x * 0x1p-60;
	}
	if (top >= 0x7ff) return __math_invalid(x);
	dd_t r;
	int n = rem_pio2_inline(x, &r);
	return sin_quadrant(n, r);
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
	int n = rem_pio2_inline(x, &r);
	return sin_quadrant(n + 1, r);           /* cos x = sin(x + pi/2) */
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
	int n = rem_pio2_inline(x, &r);
	double sv = sin_k(r), cv = cos_k(r);
	*s = negate_if(n & 2, pick(n, sv, cv));
	*c = negate_if((n + 1) & 2, pick(n, cv, sv));
}

double tan(double x)
{
	uint32_t top = TOP12(x) & 0x7ff;
	if (top < 0x3e5) {
		if (top < 0x010) fp_force_eval(x * x);
		/* the true value lies strictly beyond/before x by far less than
		 * half an ulp: nudge so that directed rounding goes the right way
		 * (to nearest this is x itself); zero keeps its sign */
		return x == 0 ? x : x + x * 0x1p-60;
	}
	if (top >= 0x7ff) return __math_invalid(x);
	dd_t r;
	int n = rem_pio2_inline(x, &r);
	dd_t s = sin_dd(r), c = cos_dd(r);
	/* tan = s/c in even quadrants, -c/s in odd ones */
	dd_t num = { pick(n, s.hi, c.hi), pick(n, s.lo, c.lo) };
	dd_t den = { pick(n, c.hi, s.hi), pick(n, c.lo, s.lo) };
	dd_t t = dd_div(num, den);
	return negate_if(n & 1, t.hi + t.lo);
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
		kd = (double)(int64_t)(2.0 * x + __builtin_copysign(0.5, x));
		rr = x - 0.5 * kd;
	}
	int64_t k = (int64_t)kd;
	dd_t r = two_prod(rr, __pi_dd[0]);
	r.lo += rr * __pi_dd[1];
	r = fast_two_sum(r.hi, r.lo);
	switch (k & 3) {
	case 0: return sin_dd(r);
	case 1: return cos_dd(r);
	case 2: return dd_neg(sin_dd(r));
	default: return dd_neg(cos_dd(r));
	}
}

double __sinpi(double x)
{
	dd_t r = __sinpi_dd(x);
	return r.hi + r.lo;
}
