/*
 * lib-spfxd — pow.
 *
 * pow(x, y) = exp(y log|x|) with log|x| as a double-double (relative error
 * < 2^-68) and y log|x| formed exactly as a dd, so the final exponential
 * sees an argument accurate to ~2^-58 absolute even at the overflow
 * threshold: results are within 0.52 ulp.  All special cases follow
 * C11 Annex F.9.4.4.
 */
#include <errno.h>
#include <math.h>
#include "fp.h"
#include "fastpath.h"

/* 0: not an integer, 1: odd integer, 2: even integer (y finite) */
static int int_kind(double y)
{
	uint64_t u = asuint64(y);
	int e = (int)(u >> 52 & 0x7ff);
	if (e < 0x3ff) return (u << 1) ? 0 : 2;     /* |y| < 1: only 0 is integral */
	if (e > 0x3ff + 52) return 2;
	uint64_t frac = 1ULL << (0x3ff + 52 - e);
	if (u & (frac - 1)) return 0;
	return (u & frac) ? 1 : 2;
}

/* exp(y log x) when its rounding is certain: returns 1 and the (signed)
 * result in *res, or 0.  Error budget in fastpath.h (log_acc_core,
 * exp_fast_core_f): an error et in y log x is at most 2.01 et absolute on
 * the scale of hi < 2. */
static always_inline int pow_fast(int F, double x, double y, int sign, double *res)
{
	double el;
	dd_t L = log_acc_core_f(F, x, &el);
	dd_t t = two_prod_f(F, y, L.hi);
	double ylo = y * L.lo;
	t.lo += ylo;
	if (unlikely(!(fabs(t.hi) <= 708.0))) return 0;
	/* error of t: |y| el, plus two roundings in t.lo */
	double et = fabs(y) * el + 0x1p-51 * (fabs(t.lo) + fabs(ylo));
	double hi, lo, eps;
	int sc = exp_fast_core_f(F, t.hi, t.lo, &hi, &lo, &eps);
	/* the sign goes in before rounding (exact), for directed modes */
	if (sign) { hi = -hi; lo = -lo; }
	double a;
	if (!ziv_round(hi, lo, eps + 2.01 * et, &a)) return 0;
	*res = scale_normal(a, sc);
	return 1;
}

static FMA_TARGET int pow_fast_fma(double x, double y, int sign, double *res)
{
	return pow_fast(1, x, y, sign, res);
}

double pow(double x, double y)
{
	uint64_t ux = asuint64(x), uy = asuint64(y);
	int sign = 0;

	if (unlikely(!(uy << 1))) return 1.0;               /* y == +-0 */
	if (unlikely(ux == 0x3ff0000000000000ULL)) return 1.0;  /* x == 1 */
	if (unlikely(__builtin_isnan(x) || __builtin_isnan(y))) return x + y;

	if (unlikely(__builtin_isinf(y))) {
		double ax = fabs(x);
		if (ax == 1.0) return 1.0;
		if ((ax < 1.0) == (y < 0)) return __builtin_inf();
		return 0.0;
	}
	int yk = int_kind(y);
	if (unlikely(!(ux << 1) || __builtin_isinf(x))) {
		int neg = (int)(ux >> 63) && yk == 1;
		if (x == 0) {
			if (y < 0) return __math_divzero((uint32_t)neg);
			return neg ? -0.0 : 0.0;
		}
		double r = (y < 0) ? 0.0 : __builtin_inf();
		return neg ? -r : r;
	}
	if (ux >> 63) {
		if (!yk) return __math_invalid(x);
		sign = yk == 1;
		x = -x;
	}
	if (unlikely(fabs(y) >= 0x1p64)) {
		/* |log x| >= 2^-53 for x != 1, so |y log x| >= 2^11 */
		if (x == 1.0) return 1.0;               /* (-1)^(huge even) */
		if ((x > 1.0) == (y > 0)) return __math_oflow((uint32_t)sign);
		return __math_uflow((uint32_t)sign);
	}
	/* simple exact cases that are worth not rounding at all */
	if (y == 1.0) return sign ? -x : x;
	if (y == 2.0) return __math_range(x * x);
	if (y == -1.0 && x != 0) return __math_range(sign ? -1.0 / x : 1.0 / x);
	if (y == 0.5 && !sign) return __builtin_sqrt(x);
	/* x a normal power of two and y integral: the result 2^(k y) is exact
	 * whenever it is representable, so no inexact or spurious underflow */
	if (yk && !(asuint64(x) << 12) && EXP_BITS(x) && fabs(y) < 0x1p20) {
		double r = scalbn(1.0, (EXP_BITS(x) - 0x3ff) * (int)y);
		return sign ? -r : r;
	}

	/* fast path: x normal, result normal (|y log x| <= 708) */
	if (likely(EXP_BITS(x) != 0)) {
		double r;
		if (likely(HAVE_FMA() ? pow_fast_fma(x, y, sign, &r) : pow_fast(0, x, y, sign, &r))) return r;
	}
	dd_t l = __log_dd(x);
	dd_t t = two_prod(y, l.hi);
	t.lo += y * l.lo;
	t = fast_two_sum(t.hi, t.lo);
	if (t.hi == 0 && t.lo == 0) return sign ? -1.0 : 1.0;
	return __exp_dd(t, sign);
}
