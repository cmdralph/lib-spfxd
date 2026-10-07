/*
 * lib-spfxd — internal floating-point toolkit for libm.
 *
 *   bit access          asuint64 / asdouble / asuint / asfloat
 *   evaluation control  fp_barrier (hide a value from constant folding),
 *                       fp_force_eval (make an exception-raising
 *                       operation happen even if its result is unused)
 *   double-double       error-free transformations (two_sum, two_prod)
 *                       and a small set of dd operations; every libm
 *                       routine that needs more than 53 bits uses these
 *   error reporting     __math_* helpers: set errno and raise the IEEE
 *                       exception through real arithmetic
 *
 * The library is compiled with -ffp-contract=off so a*b+c is never fused
 * behind our back (the error-free transformations depend on it) and with
 * -frounding-math so constant folding never assumes round-to-nearest.
 */
#ifndef _SPFXD_FP_H
#define _SPFXD_FP_H

#include <stdint.h>
#include <float.h>
#include "libc.h"

/* The library is compiled with -ffreestanding (hence -fno-builtin), which
 * would turn every fabs/copysign in libm into a function call.  Inside
 * the library they are bit operations; the files that define the public
 * functions opt out with SPFXD_DEFINES_FP_PRIMITIVES. */
#ifndef SPFXD_DEFINES_FP_PRIMITIVES
#undef fabs
#undef fabsf
#undef fabsl
#undef copysign
#undef copysignf
#undef copysignl
#define fabs(x) __builtin_fabs(x)
#define fabsf(x) __builtin_fabsf(x)
#define fabsl(x) __builtin_fabsl(x)
#define copysign(x, y) __builtin_copysign(x, y)
#define copysignf(x, y) __builtin_copysignf(x, y)
#define copysignl(x, y) __builtin_copysignl(x, y)
#endif

static always_inline uint64_t asuint64(double x)
{
	union { double f; uint64_t i; } u = { x };
	return u.i;
}

static always_inline double asdouble(uint64_t i)
{
	union { uint64_t i; double f; } u = { i };
	return u.f;
}

static always_inline uint32_t asuint(float x)
{
	union { float f; uint32_t i; } u = { x };
	return u.i;
}

static always_inline float asfloat(uint32_t i)
{
	union { uint32_t i; float f; } u = { i };
	return u.f;
}

/* x87 80-bit extended: 64-bit significand with explicit integer bit,
 * 15-bit exponent with the sign in bit 15 of se. */
union ldshape {
	long double f;
	struct {
		uint64_t m;
		uint16_t se;
	} i;
};

static always_inline double fp_barrier(double x)
{
	volatile double y = x;
	return y;
}

static always_inline float fp_barrierf(float x)
{
	volatile float y = x;
	return y;
}

static always_inline long double fp_barrierl(long double x)
{
	volatile long double y = x;
	return y;
}

static always_inline void fp_force_eval(double x)
{
	volatile double y;
	y = x;
	(void)y;
}

static always_inline void fp_force_evalf(float x)
{
	volatile float y;
	y = x;
	(void)y;
}

static always_inline void fp_force_evall(long double x)
{
	volatile long double y;
	y = x;
	(void)y;
}

/* Top 12 bits (sign + exponent) and the biased exponent alone. */
#define TOP12(x) ((uint32_t)(asuint64(x) >> 52))
#define EXP_BITS(x) ((int)((asuint64(x) >> 52) & 0x7ff))

/* ---------------------------------------------------------------------
 * Double-double arithmetic.  A dd value is hi + lo with |lo| <= ulp(hi)/2.
 * ------------------------------------------------------------------- */
typedef struct { double hi, lo; } dd_t;

/* s + e == a + b exactly, s = fl(a + b), any magnitudes. */
static always_inline dd_t two_sum(double a, double b)
{
	double s = a + b;
	double bb = s - a;
	double e = (a - (s - bb)) + (b - bb);
	return (dd_t){ s, e };
}

/* Same, requiring |a| >= |b| (or a == 0). */
static always_inline dd_t fast_two_sum(double a, double b)
{
	double s = a + b;
	return (dd_t){ s, b - (s - a) };
}

/* p + e == a * b exactly (barring underflow), p = fl(a * b).  The error
 * term comes from a hardware FMA when the CPU has one (checked at run
 * time on x86-64; the branch is perfectly predictable), otherwise from
 * splitting the operands into 26-bit halves (Veltkamp/Dekker).  Both are
 * exact, so results do not depend on which is used. */
static always_inline double two_prod_err_split(double a, double b, double p)
{
	const double split = 134217729.0;   /* 2^27 + 1 */
	double ca = split * a, cb = split * b;
	double ah = ca - (ca - a), al = a - ah;
	double bh = cb - (cb - b), bl = b - bh;
	return ((ah * bh - p) + ah * bl + al * bh) + al * bl;
}

static always_inline dd_t two_prod(double a, double b)
{
	double p = a * b;
#if defined(__FMA__)
	double e = __builtin_fma(a, b, -p);
#elif defined(__x86_64__) && !defined(SPFXD_NO_RUNTIME_FMA)
	double e;
	if (likely(__cpu_features & 8 /* CPU_FMA */)) {
		e = p;
		/* e = a * b - e, one rounding */
		__asm__ ("vfmsub231sd %2, %1, %0" : "+x"(e) : "x"(a), "x"(b));
	} else {
		e = two_prod_err_split(a, b, p);
	}
#else
	double e = two_prod_err_split(a, b, p);
#endif
	return (dd_t){ p, e };
}

static always_inline dd_t dd_add(dd_t a, dd_t b)
{
	dd_t s = two_sum(a.hi, b.hi);
	dd_t t = two_sum(a.lo, b.lo);
	s.lo += t.hi;
	s = fast_two_sum(s.hi, s.lo);
	s.lo += t.lo;
	return fast_two_sum(s.hi, s.lo);
}

static always_inline dd_t dd_add_d(dd_t a, double b)
{
	dd_t s = two_sum(a.hi, b);
	s.lo += a.lo;
	return fast_two_sum(s.hi, s.lo);
}

static always_inline dd_t dd_mul(dd_t a, dd_t b)
{
	dd_t p = two_prod(a.hi, b.hi);
	p.lo += a.hi * b.lo + a.lo * b.hi;
	return fast_two_sum(p.hi, p.lo);
}

static always_inline dd_t dd_mul_d(dd_t a, double b)
{
	dd_t p = two_prod(a.hi, b);
	p.lo += a.lo * b;
	return fast_two_sum(p.hi, p.lo);
}

static always_inline dd_t dd_div(dd_t a, dd_t b)
{
	double q1 = a.hi / b.hi;
	dd_t r = dd_mul_d(b, q1);
	dd_t d = two_sum(a.hi, -r.hi);
	d.lo -= r.lo;
	d.lo += a.lo;
	double q2 = (d.hi + d.lo) / b.hi;
	return fast_two_sum(q1, q2);
}

static always_inline dd_t dd_sqrt(dd_t a)
{
	double s = __builtin_sqrt(a.hi);
	dd_t sq = two_prod(s, s);
	double c = ((a.hi - sq.hi) - sq.lo + a.lo) / (2 * s);
	return fast_two_sum(s, c);
}

static always_inline dd_t dd_neg(dd_t a)
{
	return (dd_t){ -a.hi, -a.lo };
}

static always_inline dd_t dd_from(double a)
{
	return (dd_t){ a, 0.0 };
}

static always_inline dd_t dd_c(const double *c)
{
	return (dd_t){ c[0], c[1] };
}

/* Nearest integer to z (|z| < 2^31), ties away from zero, independent of
 * the rounding mode and without a branch on the sign (arguments of the
 * reduction steps are random in sign, so a branch would mispredict). */
static always_inline int iround(double z)
{
	return (int)(z + __builtin_copysign(0.5, z));
}

/* Exact 2^k for normal-range k. */
static always_inline double pow2i(int k)
{
	return asdouble((uint64_t)(k + 1023) << 52);
}

/* ---------------------------------------------------------------------
 * Error handling (src/math/math_err.c).  Each returns the value the
 * caller should return, after setting errno and raising the exception.
 * ------------------------------------------------------------------- */
hidden double __math_invalid(double x);            /* EDOM, NaN */
hidden double __math_divzero(uint32_t sign);        /* ERANGE, +-inf */
hidden double __math_oflow(uint32_t sign);          /* ERANGE, +-inf */
hidden double __math_uflow(uint32_t sign);          /* ERANGE, +-0 */
hidden double __math_check_oflow(double y);
hidden double __math_check_uflow(double y);
hidden double __math_range(double y);               /* checks both */
hidden float __math_invalidf(float x);
hidden float __math_divzerof(uint32_t sign);
hidden float __math_oflowf(uint32_t sign);
hidden float __math_uflowf(uint32_t sign);
hidden float __math_narrowf(double y);              /* round to float, check range */
hidden long double __math_invalidl(long double x);
hidden long double __math_divzerol(uint32_t sign);
hidden long double __math_oflowl(uint32_t sign);
hidden long double __math_uflowl(uint32_t sign);
hidden long double __math_rangel(long double y);

/* ---------------------------------------------------------------------
 * Shared kernels.
 * ------------------------------------------------------------------- */
/* exp(x.hi + x.lo) * 2^scale as a double, overflow/underflow handled
 * (errno set) for any input; sign 1 negates the result. */
hidden double __exp_dd(dd_t x, int sign);
/* exp(x) as an unevaluated dd, |x| <= 708; result relative error < 2^-63. */
hidden dd_t __exp_dd_kernel(dd_t x, int *scale);
/* round e * 2^k (negated if sign) to double with one rounding in the
 * normal range; errno set on overflow/underflow. */
hidden double __exp_finish(dd_t e, int k, int sign);
/* log(x) for finite positive normal-or-subnormal x as dd, error < 2^-68. */
hidden dd_t __log_dd(double x);
/* x mod pi/2: returns n (mod 4 is all that matters) and the remainder
 * as a dd with |r| <= pi/4 (slightly more for huge x). */
hidden int __rem_pio2(double x, dd_t *r);
/* Payne-Hanek core shared with long double: x = m 2^e (e >= -40) */
hidden int __rem_pio2_bits(uint64_t m, int e, uint64_t f[2], int *fneg);
/* sin/cos of a reduced argument |r| <= ~pi/4 as dd. */
hidden dd_t __sin_dd(dd_t r);
hidden dd_t __cos_dd(dd_t r);
hidden double __sin_kernel(dd_t r);
hidden double __cos_kernel(dd_t r);
/* atan of a dd argument (any magnitude), as dd. */
hidden dd_t __atan_dd(dd_t x);
/* sin(pi x) for finite x, exact argument reduction. */
hidden double __sinpi(double x);
hidden dd_t __sinpi_dd(double x);
/* lgamma for finite x with sign of gamma in *sg, as dd when finite. */
hidden double __lgamma_r(double x, int *sg);
/* x87 helpers: 2^x for |x| <= 1/2 etc. (arch/x86_64/src/math). */

/* Data tables (src/math/math_data.c, generated by tools/gen-math.py). */
extern hidden const double __exp_tab[64], __exp_ln2_32[2], __exp_inv_ln2_32, __expm1_poly[5];
extern hidden const double __log_inv[128], __log_logc[256], __log_ln2[2], __log1p_poly[8];
extern hidden const double __inv_ln2[2], __inv_ln10[2], __ln10[2], __log2_10[2], __ln2[2], __log10_2[2];
extern hidden const double __sin_poly[7], __cos_poly[7];
extern hidden const double __pio2_split[4], __pio2_dd[2], __pi_dd[2], __two_over_pi;
extern hidden const uint64_t __two_over_pi_bits[264];
extern hidden const double __atan_tab[34], __atan_poly[6];
extern hidden const double __sinh_poly[7], __tanh_poly[12];
extern hidden const double __erf_poly[12], __erfc_bounds[9], __erfc_coef[129], __erfc_asym[15], __erfc_lo[16];
extern hidden const int __erfc_degree[8], __erfc_start[8];
extern hidden const double __lgamma1_poly[21], __lgamma2_poly[28], __stirling[10];
extern hidden const double __half_log_2pi[2], __log_pi[2], __euler_gamma, __euler_dd[2];
extern hidden const double __two_over_pi_dd[2], __inv_pi_dd[2];
extern hidden const double __two_over_sqrtpi[2], __inv_sqrtpi[2], __sqrt_2_over_pi[2];
extern hidden const long double __pio2l_split[4], __pio2l_dd[2], __pil_dd[2], __two_over_pil;
extern hidden const long double __log2el[2], __log2_10l[2], __ln2l[2], __log10_2l[2];
extern hidden const long double __exp2l_tab[130];

/* c[0] + c[1] x + ... + c[n] x^n.
 *
 * With a constant degree the polynomial is evaluated by Estrin's scheme:
 * pairs (c[i] + c[i+1] x) combined with x^2, x^4, x^8, x^16, so the
 * dependency chain is about log2(n) multiply-adds long instead of n
 * (Horner).  The rounding error is of the same order as Horner's for the
 * small arguments every caller uses.  A variable degree falls back to
 * Horner's rule. */
static always_inline double poly_e3(const double *c, int n, double x, double x2)
{
	switch (n) {
	case 0: return c[0];
	case 1: return c[0] + c[1] * x;
	case 2: return (c[0] + c[1] * x) + x2 * c[2];
	default: return (c[0] + c[1] * x) + x2 * (c[2] + c[3] * x);
	}
}

static always_inline double poly_e7(const double *c, int n, double x, double x2, double x4)
{
	if (n <= 3) return poly_e3(c, n, x, x2);
	return poly_e3(c, 3, x, x2) + x4 * poly_e3(c + 4, n - 4, x, x2);
}

static always_inline double poly_e15(const double *c, int n, double x, double x2, double x4, double x8)
{
	if (n <= 7) return poly_e7(c, n, x, x2, x4);
	return poly_e7(c, 7, x, x2, x4) + x8 * poly_e7(c + 8, n - 8, x, x2, x4);
}

static always_inline double poly_e31(const double *c, int n, double x, double x2, double x4, double x8,
	double x16)
{
	if (n <= 15) return poly_e15(c, n, x, x2, x4, x8);
	return poly_e15(c, 15, x, x2, x4, x8) + x16 * poly_e15(c + 16, n - 16, x, x2, x4, x8);
}

static always_inline double poly_horner(const double *c, int n, double x)
{
	double r = c[n];
	for (int i = n - 1; i >= 0; i--) r = r * x + c[i];
	return r;
}

static always_inline double poly(const double *c, int n, double x)
{
	if (!__builtin_constant_p(n) || n > 31) return poly_horner(c, n, x);
	double x2 = x * x, x4 = x2 * x2, x8 = x4 * x4, x16 = x8 * x8;
	return poly_e31(c, n, x, x2, x4, x8, x16);
}

#endif
