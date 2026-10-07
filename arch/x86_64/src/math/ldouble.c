/*
 * lib-spfxd — long double (x87 80-bit extended) math functions.
 *
 * Elementary functions use the x87 transcendental instructions on
 * carefully prepared arguments:
 *   fyl2x / fyl2xp1   logarithms (fyl2xp1 near 1, where it is exact-ish)
 *   f2xm1 + fscale    exponentials; the argument y = x log2(e) is formed
 *                     as an extended double-double so 2^frac(y) sees a
 *                     correctly reduced argument
 *   fsin/fcos/fptan   only on |r| <= pi/4 after our own reduction modulo
 *                     pi/2 (Cody-Waite, or Payne-Hanek for huge |x|) —
 *                     the hardware's 66-bit pi is never relied upon
 *   fpatan            atan/atan2 (all quadrants, signed zeros, infinities)
 *   fprem/fprem1      exact fmodl/remainderl/remquol
 * powl computes log2(x) to ~2^-80 in extended double-double through a
 * 2^(j/64) table and an atanh series, then exponentiates the dd product.
 * Typical errors are within 1-2 ulp of extended precision.
 *
 * erfl, erfcl, lgammal, tgammal and the Bessel functions are evaluated
 * by the double implementations (see README: about 53 bits of accuracy).
 */
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include "fp.h"

typedef struct { long double hi, lo; } ldd_t;

/* ------------------------------------------------------------------ x87 */

static always_inline long double x_fyl2x(long double y, long double x)
{
	long double r;
	__asm__ ("fyl2x" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
	return r;
}

static always_inline long double x_fyl2xp1(long double y, long double x)
{
	long double r;
	__asm__ ("fyl2xp1" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
	return r;
}

static always_inline long double x_f2xm1(long double x)
{
	long double r;
	__asm__ ("f2xm1" : "=t"(r) : "0"(x));
	return r;
}

static always_inline long double x_fscale(long double x, long double n)
{
	long double r;
	__asm__ ("fscale" : "=t"(r) : "0"(x), "u"(n));
	return r;
}

static always_inline long double x_fpatan(long double y, long double x)
{
	long double r;
	__asm__ ("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
	return r;
}

static always_inline long double x_fsin(long double x)
{
	long double r;
	__asm__ ("fsin" : "=t"(r) : "0"(x));
	return r;
}

static always_inline long double x_fcos(long double x)
{
	long double r;
	__asm__ ("fcos" : "=t"(r) : "0"(x));
	return r;
}

static always_inline long double x_ftan(long double x)
{
	long double r, one;
	__asm__ ("fptan" : "=t"(one), "=u"(r) : "0"(x));
	(void)one;
	return r;
}

static always_inline long double x_fsqrt(long double x)
{
	long double r;
	__asm__ ("fsqrt" : "=t"(r) : "0"(x));
	return r;
}

/* ---------------------------------------------------- extended dd tools */

static always_inline ldd_t l_two_sum(long double a, long double b)
{
	long double s = a + b, bb = s - a;
	return (ldd_t){ s, (a - (s - bb)) + (b - bb) };
}

static always_inline ldd_t l_fast_two_sum(long double a, long double b)
{
	long double s = a + b;
	return (ldd_t){ s, b - (s - a) };
}

static always_inline ldd_t l_two_prod(long double a, long double b)
{
	const long double split = 4294967297.0L;    /* 2^32 + 1 */
	long double p = a * b;
	long double ca = split * a, cb = split * b;
	long double ah = ca - (ca - a), al = a - ah;
	long double bh = cb - (cb - b), bl = b - bh;
	return (ldd_t){ p, ((ah * bh - p) + ah * bl + al * bh) + al * bl };
}

static always_inline ldd_t l_mul(ldd_t a, ldd_t b)
{
	ldd_t p = l_two_prod(a.hi, b.hi);
	p.lo += a.hi * b.lo + a.lo * b.hi;
	return l_fast_two_sum(p.hi, p.lo);
}

static always_inline ldd_t l_mul_l(ldd_t a, long double b)
{
	ldd_t p = l_two_prod(a.hi, b);
	p.lo += a.lo * b;
	return l_fast_two_sum(p.hi, p.lo);
}

static always_inline ldd_t l_add(ldd_t a, ldd_t b)
{
	ldd_t s = l_two_sum(a.hi, b.hi);
	s.lo += a.lo + b.lo;
	return l_fast_two_sum(s.hi, s.lo);
}

static always_inline ldd_t l_div(ldd_t a, ldd_t b)
{
	long double q1 = a.hi / b.hi;
	ldd_t r = l_mul_l(b, q1);
	ldd_t d = l_two_sum(a.hi, -r.hi);
	d.lo += a.lo - r.lo;
	long double q2 = (d.hi + d.lo) / b.hi;
	return l_fast_two_sum(q1, q2);
}

static always_inline ldd_t l_c(const long double *c)
{
	return (ldd_t){ c[0], c[1] };
}

static int ld_exp(long double x)
{
	union ldshape u = { x };
	return u.i.se & 0x7fff;
}

/* ----------------------------------------------------------- manipulation */

long double fabsl(long double x)
{
	union ldshape u = { x };
	u.i.se &= 0x7fff;
	return u.f;
}

long double copysignl(long double x, long double y)
{
	union ldshape u = { x }, v = { y };
	u.i.se = (uint16_t)((u.i.se & 0x7fff) | (v.i.se & 0x8000));
	return u.f;
}

long double frexpl(long double x, int *e)
{
	union ldshape u = { x };
	int ee = u.i.se & 0x7fff;
	if (!ee) {
		if (x != 0) {
			x = frexpl(x * 0x1p120L, e);
			*e -= 120;
		} else {
			*e = 0;
		}
		return x;
	}
	if (ee == 0x7fff) {
		*e = 0;
		return x + x;
	}
	*e = ee - 0x3ffe;
	u.i.se = (uint16_t)((u.i.se & 0x8000) | 0x3ffe);
	return u.f;
}

static long double pow2l(int n)       /* exact 2^n, -16382 <= n <= 16383 */
{
	union ldshape u;
	u.i.m = 1ULL << 63;
	u.i.se = (uint16_t)(0x3fff + n);
	return u.f;
}

long double scalbnl(long double x, int n)
{
	long double x0 = x;
	if (n > 16383) {
		x *= 0x1p16383L;
		n -= 16383;
		if (n > 16383) {
			x *= 0x1p16383L;
			n -= 16383;
			if (n > 16383) n = 16383;
		}
	} else if (n < -16382) {
		x *= 0x1p-16382L * 0x1p64L;
		n += 16382 - 64;
		if (n < -16382) {
			x *= 0x1p-16382L * 0x1p64L;
			n += 16382 - 64;
			if (n < -16382) n = -16382;
		}
	}
	long double y = x * pow2l(n);
	if (__builtin_isinf(y) && !__builtin_isinf(x0)) errno = ERANGE;
	else if (y == 0 && x0 != 0) errno = ERANGE;
	return y;
}

long double ldexpl(long double x, int n)
{
	return scalbnl(x, n);
}

long double scalblnl(long double x, long n)
{
	if (n > INT_MAX) n = INT_MAX;
	else if (n < INT_MIN) n = INT_MIN;
	return scalbnl(x, (int)n);
}

int ilogbl(long double x)
{
	union ldshape u = { x };
	int e = u.i.se & 0x7fff;
	if (!e) {
		if (!u.i.m) {
			fp_force_eval(fp_barrier(0.0) / 0.0);
			errno = EDOM;
			return FP_ILOGB0;
		}
		return -16382 - __builtin_clzll(u.i.m);
	}
	if (e == 0x7fff) {
		fp_force_eval(fp_barrier(0.0) / 0.0);
		errno = EDOM;
		return (u.i.m << 1) ? FP_ILOGBNAN : INT_MAX;
	}
	return e - 0x3fff;
}

long double logbl(long double x)
{
	if (!__builtin_isfinite(x)) return x * x;
	if (x == 0) return fp_barrierl(-1.0L) / 0.0L;
	return (long double)ilogbl(x);
}

long double modfl(long double x, long double *ip)
{
	long double t = truncl(x);
	*ip = t;
	if (__builtin_isnan(x)) return x + x;
	if (__builtin_isinf(x)) return copysignl(0.0L, x);
	return copysignl(x - t, x);
}

long double nextafterl(long double x, long double y)
{
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	if (x == y) return y;
	union ldshape u = { x };
	if (x == 0) {
		u.f = copysignl(0x1p-16445L, y);
	} else if ((x < y) == !(u.i.se >> 15)) {
		/* away from zero */
		u.i.m++;
		if (u.i.m == 0) {
			u.i.m = 1ULL << 63;
			u.i.se++;
		} else if ((u.i.se & 0x7fff) == 0 && (u.i.m >> 63)) {
			u.i.se++;                       /* subnormal became normal */
		}
	} else {
		/* toward zero */
		if ((u.i.se & 0x7fff) && u.i.m == (1ULL << 63)) {
			u.i.se--;
			u.i.m = (u.i.se & 0x7fff) ? ~0ULL : (~0ULL >> 1);
		} else {
			u.i.m--;
		}
	}
	int e = u.i.se & 0x7fff;
	if (e == 0x7fff) {
		fp_force_evall(x + x);
		errno = ERANGE;
	} else if (!e) {
		fp_force_evall(x * x + u.f * u.f);
		if (x != 0) errno = ERANGE;
	}
	return u.f;
}

long double nexttowardl(long double x, long double y)
{
	return nextafterl(x, y);
}

long double nanl(const char *s)
{
	return (long double)nan(s);
}

long double fdiml(long double x, long double y)
{
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	return x > y ? x - y : 0.0L;
}

long double fmaxl(long double x, long double y)
{
	if (__builtin_isnan(x)) return y;
	if (__builtin_isnan(y)) return x;
	if (__builtin_signbit(x) != __builtin_signbit(y)) return __builtin_signbit(x) ? y : x;
	return x < y ? y : x;
}

long double fminl(long double x, long double y)
{
	if (__builtin_isnan(x)) return y;
	if (__builtin_isnan(y)) return x;
	if (__builtin_signbit(x) != __builtin_signbit(y)) return __builtin_signbit(x) ? x : y;
	return x < y ? x : y;
}

long double significandl(long double x)
{
	if (!__builtin_isfinite(x) || x == 0) return x;
	return scalbnl(x, -ilogbl(x));
}

int finitel(long double x) { return __builtin_isfinite(x); }
int __finitel(long double x) { return __builtin_isfinite(x); }
int __isinfl(long double x) { return __builtin_isinf_sign(x); }
int __isnanl(long double x) { return __builtin_isnan(x); }
int __signbitl(long double x) { union ldshape u = { x }; return u.i.se >> 15; }
int __fpclassifyl(long double x) { return fpclassify(x); }

long double sqrtl(long double x)
{
	if (x < 0) return __math_invalidl(x);
	return x_fsqrt(x);
}

long lrintl(long double x)
{
	long r;
	__asm__ ("fistpll %0" : "=m"(r) : "t"(x) : "st");
	return r;
}

long long llrintl(long double x)
{
	return lrintl(x);
}

/* --------------------------------------------------------- remainders */

long double fmodl(long double x, long double y)
{
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	if (__builtin_isinf(x) || y == 0) return __math_invalidl(x * y);
	if (__builtin_isinf(y)) return x;
	long double r;
	unsigned short sw;
	__asm__ ("1: fprem\n\tfnstsw %%ax\n\ttestl $0x400, %%eax\n\tjnz 1b"
	         : "=t"(r), "=a"(sw) : "0"(x), "u"(y));
	return r;
}

long double remquol(long double x, long double y, int *quo)
{
	*quo = 0;
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	if (__builtin_isinf(x) || y == 0) return __math_invalidl(x * y);
	if (__builtin_isinf(y)) return x;
	long double r;
	unsigned sw;
	__asm__ ("1: fprem1\n\tfnstsw %%ax\n\ttestl $0x400, %%eax\n\tjnz 1b"
	         : "=t"(r), "=a"(sw) : "0"(x), "u"(y));
	int q = (int)(((sw >> 8) & 1) << 2 | ((sw >> 14) & 1) << 1 | ((sw >> 9) & 1));
	*quo = (__builtin_signbit(x) != __builtin_signbit(y)) ? -q : q;
	return r;
}

long double remainderl(long double x, long double y)
{
	int q;
	return remquol(x, y, &q);
}

long double dreml(long double x, long double y)
{
	return remainderl(x, y);
}

/* ------------------------------------------------------- exponentials */

/* 2^(y.hi + y.lo) with errno handling; sign negates the result. */
static long double exp2_dd(ldd_t y, int sign)
{
	if (y.hi >= 16384.0L) return __math_oflowl((uint32_t)sign);
	if (y.hi < -16446.0L) return __math_uflowl((uint32_t)sign);
	long double n = rintl(y.hi);
	long double f = (y.hi - n) + y.lo;      /* y.hi - n is exact */
	if (f > 0.5L) { f -= 1.0L; n += 1.0L; }
	else if (f < -0.5L) { f += 1.0L; n -= 1.0L; }
	long double m = x_f2xm1(f) + 1.0L;
	if (sign) m = -m;
	long double r;
	if (n < -16382.0L) {
		/* subnormal result: scale in two steps to round once */
		r = x_fscale(m, n + 100.0L) * 0x1p-100L;
	} else {
		r = x_fscale(m, n);
	}
	return __math_rangel(r);
}

static ldd_t mul_const(long double x, const long double *c)
{
	ldd_t p = l_two_prod(x, c[0]);
	p.lo += x * c[1];
	return l_fast_two_sum(p.hi, p.lo);
}

long double expl(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (__builtin_isinf(x)) return x > 0 ? x : 0.0L;
	if (fabsl(x) < 0x1p-66L) return 1.0L + x;
	return exp2_dd(mul_const(x, __log2el), 0);
}

long double exp2l(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (__builtin_isinf(x)) return x > 0 ? x : 0.0L;
	return exp2_dd((ldd_t){ x, 0.0L }, 0);
}

long double exp10l(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (__builtin_isinf(x)) return x > 0 ? x : 0.0L;
	if (fabsl(x) < 0x1p-66L) return 1.0L + x;
	if (x > 4933.0L) return __math_oflowl(0);
	if (x < -4951.0L) return __math_uflowl(0);
	return exp2_dd(mul_const(x, __log2_10l), 0);
}

long double pow10l(long double x)
{
	return exp10l(x);
}

long double expm1l(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (__builtin_isinf(x)) return x > 0 ? x : -1.0L;
	long double a = fabsl(x);
	if (a < 0x1p-65L) {
		if (x != 0) fp_force_evall(x * x + 0x1p-16000L);
		return x;
	}
	if (x < -46.0L) return fp_barrierl(0x1p-16000L) - 1.0L;
	if (x > 11356.6L) return __math_oflowl(0);
	ldd_t y = mul_const(x, __log2el);
	if (fabsl(y.hi) <= 0.5L) {
		/* 2^y - 1 = f2xm1(y.hi) + y.lo ln2 2^y.hi */
		long double e = x_f2xm1(y.hi);
		return e + y.lo * __ln2l[0] * (1.0L + e);
	}
	long double n = rintl(y.hi);
	long double f = (y.hi - n) + y.lo;
	long double e = x_f2xm1(f);
	if (n > 16000.0L) return __math_rangel(x_fscale(e + 1.0L, n));
	/* 2^n (e + 1) - 1 = 2^n e + (2^n - 1) */
	long double p = x_fscale(1.0L, n);
	return p * e + (p - 1.0L);
}

/* --------------------------------------------------------- logarithms */

static int log_special(long double x, long double *r)
{
	if (__builtin_isnan(x)) { *r = x + x; return 1; }
	if (x < 0) { *r = __math_invalidl(x); return 1; }
	if (x == 0) { *r = __math_divzerol(1); return 1; }
	if (__builtin_isinf(x)) { *r = x; return 1; }
	return 0;
}

long double logl(long double x)
{
	long double r;
	if (log_special(x, &r)) return r;
	if (fabsl(x - 1.0L) < 0.29L) return x_fyl2xp1(__ln2l[0], x - 1.0L);
	return x_fyl2x(__ln2l[0], x);
}

long double log2l(long double x)
{
	long double r;
	if (log_special(x, &r)) return r;
	if (fabsl(x - 1.0L) < 0.29L) return x_fyl2xp1(1.0L, x - 1.0L);
	return x_fyl2x(1.0L, x);
}

long double log10l(long double x)
{
	long double r;
	if (log_special(x, &r)) return r;
	if (fabsl(x - 1.0L) < 0.29L) return x_fyl2xp1(__log10_2l[0], x - 1.0L);
	return x_fyl2x(__log10_2l[0], x);
}

long double log1pl(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (x <= -1.0L) return x == -1.0L ? __math_divzerol(1) : __math_invalidl(x);
	if (__builtin_isinf(x)) return x;
	if (fabsl(x) < 0x1p-65L) {
		if (x != 0) fp_force_evall(x * x + 0x1p-16000L);
		return x;
	}
	if (fabsl(x) < 0.29L) return x_fyl2xp1(__ln2l[0], x);
	return x_fyl2x(__ln2l[0], 1.0L + x);
}

/* ------------------------------------------------------------------ pow */

/* 0: not an integer, 1: odd integer, 2: even integer (y finite) */
static int int_kindl(long double y)
{
	if (truncl(y) != y) return 0;
	if (fabsl(y) >= 0x1p64L) return 2;
	return fmodl(y, 2.0L) != 0 ? 1 : 2;
}

/* log2(x) for finite x > 0 as an extended dd with error ~2^-80 */
static ldd_t log2_dd(long double x)
{
	int k;
	long double m = frexpl(x, &k);          /* x = m 2^k, m in [0.5, 1) */
	m *= 2.0L;
	k--;
	if (m > 1.41421356237309504880L) { m *= 0.5L; k++; }
	int j = (int)rintl(64.0L * x_fyl2x(1.0L, m));   /* in [-32, 32] */
	const long double *t = __exp2l_tab + 2 * (j + 32);
	/* u = m 2^(-j/64), close to 1 */
	ldd_t u = l_two_prod(m, t[0]);
	u.lo += m * t[1];
	ldd_t w = l_two_sum(u.hi - 1.0L, u.lo);
	/* ln(1 + w) = 2 atanh(s), s = w / (2 + w) */
	ldd_t s = l_div(w, l_add((ldd_t){ 2.0L, 0.0L }, w));
	long double z = s.hi * s.hi;
	long double c = 2.0L * s.hi * z *
		(1.0L / 3 + z * (1.0L / 5 + z * (1.0L / 7 + z * (1.0L / 9 + z * (1.0L / 11)))));
	ldd_t ln = l_fast_two_sum(2.0L * s.hi, 2.0L * s.lo + c);
	ldd_t l2 = l_mul(ln, l_c(__log2el));
	return l_add((ldd_t){ (long double)k + j / 64.0L, 0.0L }, l2);
}

long double powl(long double x, long double y)
{
	int sign = 0;
	if (y == 0) return 1.0L;
	if (x == 1.0L) return 1.0L;
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	if (__builtin_isinf(y)) {
		long double ax = fabsl(x);
		if (ax == 1.0L) return 1.0L;
		if ((ax < 1.0L) == (y < 0)) return __builtin_infl();
		return 0.0L;
	}
	int yk = int_kindl(y);
	if (x == 0 || __builtin_isinf(x)) {
		int neg = __builtin_signbit(x) && yk == 1;
		if (x == 0) {
			if (y < 0) return __math_divzerol((uint32_t)neg);
			return neg ? -0.0L : 0.0L;
		}
		long double r = y < 0 ? 0.0L : __builtin_infl();
		return neg ? -r : r;
	}
	if (x < 0) {
		if (!yk) return __math_invalidl(x);
		sign = yk == 1;
		x = -x;
	}
	if (fabsl(y) >= 0x1p80L) {
		if ((x > 1.0L) == (y > 0)) return __math_oflowl((uint32_t)sign);
		return __math_uflowl((uint32_t)sign);
	}
	if (y == 1.0L) return sign ? -x : x;
	if (y == 2.0L) return __math_rangel(x * x);
	if (y == 0.5L && !sign) return x_fsqrt(x);
	ldd_t l = log2_dd(x);
	ldd_t e = l_mul_l(l, y);
	return exp2_dd(e, sign);
}

/* ------------------------------------------------------------- trig */


/* x mod pi/2 for finite x: returns n mod 4, remainder as dd */
static int rem_pio2l(long double x, ldd_t *r)
{
	long double ax = fabsl(x);
	if (ax <= 0.785398163397448309615L) {
		*r = (ldd_t){ x, 0.0L };
		return 0;
	}
	if (ax < 0x1p30L) {
		long double n = rintl(x * __two_over_pil);
		long double y1 = x - n * __pio2l_split[0];
		ldd_t a = l_two_sum(y1, -n * __pio2l_split[1]);
		ldd_t s = l_two_sum(a.hi, -n * __pio2l_split[2]);
		s.lo += a.lo - n * __pio2l_split[3];
		*r = l_fast_two_sum(s.hi, s.lo);
		return (int)((long long)n & 3);
	}
	union ldshape u = { ax };
	int e = (u.i.se & 0x7fff) - 16383 - 63;
	uint64_t f[2];
	int fneg;
	int n = __rem_pio2_bits(u.i.m, e, f, &fneg);
	uint64_t fh = f[0], fl = f[1];
	int lz = 0;
	if (!fh) { fh = fl; fl = 0; lz = 64; }
	int s = __builtin_clzll(fh);
	if (s) { fh = (fh << s) | (fl >> (64 - s)); fl <<= s; }
	lz += s;
	long double hi = scalbnl((long double)fh, -64 - lz);
	long double lo = scalbnl((long double)fl, -128 - lz);
	ldd_t res = l_mul(l_fast_two_sum(hi, lo), l_c(__pio2l_dd));
	if (fneg) res = (ldd_t){ -res.hi, -res.lo };
	if (x < 0) {
		res = (ldd_t){ -res.hi, -res.lo };
		n = -n;
	}
	*r = res;
	return n & 3;
}

static long double k_sin(ldd_t r) { return x_fsin(r.hi) + r.lo * x_fcos(r.hi); }
static long double k_cos(ldd_t r) { return x_fcos(r.hi) - r.lo * x_fsin(r.hi); }

long double sinl(long double x)
{
	if (!__builtin_isfinite(x)) return __builtin_isnan(x) ? x + x : __math_invalidl(x);
	if (fabsl(x) < 0x1p-32L) {
		if (x != 0) fp_force_evall(x + 0x1p-16000L);
		return x;
	}
	ldd_t r;
	switch (rem_pio2l(x, &r)) {
	case 0: return k_sin(r);
	case 1: return k_cos(r);
	case 2: return -k_sin(r);
	default: return -k_cos(r);
	}
}

long double cosl(long double x)
{
	if (!__builtin_isfinite(x)) return __builtin_isnan(x) ? x + x : __math_invalidl(x);
	ldd_t r;
	switch (rem_pio2l(x, &r)) {
	case 0: return k_cos(r);
	case 1: return -k_sin(r);
	case 2: return -k_cos(r);
	default: return k_sin(r);
	}
}

void sincosl(long double x, long double *s, long double *c)
{
	if (!__builtin_isfinite(x)) {
		*s = *c = __builtin_isnan(x) ? x + x : __math_invalidl(x);
		return;
	}
	ldd_t r;
	int n = rem_pio2l(x, &r);
	long double sv = k_sin(r), cv = k_cos(r);
	switch (n) {
	case 0: *s = sv; *c = cv; break;
	case 1: *s = cv; *c = -sv; break;
	case 2: *s = -sv; *c = -cv; break;
	default: *s = -cv; *c = sv; break;
	}
}

long double tanl(long double x)
{
	if (!__builtin_isfinite(x)) return __builtin_isnan(x) ? x + x : __math_invalidl(x);
	if (fabsl(x) < 0x1p-32L) {
		if (x != 0) fp_force_evall(x + 0x1p-16000L);
		return x;
	}
	ldd_t r;
	int n = rem_pio2l(x, &r);
	long double t = x_ftan(r.hi);
	t += r.lo * (1.0L + t * t);
	return (n & 1) ? -1.0L / t : t;
}

long double atanl(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	return x_fpatan(x, 1.0L);
}

long double atan2l(long double y, long double x)
{
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	return x_fpatan(y, x);
}

long double asinl(long double x)
{
	long double a = fabsl(x);
	if (!__builtin_islessequal(a, 1.0L)) return __builtin_isnan(x) ? x + x : __math_invalidl(x);
	if (a < 0x1p-32L) return x;
	return x_fpatan(x, x_fsqrt((1.0L - x) * (1.0L + x)));
}

long double acosl(long double x)
{
	long double a = fabsl(x);
	if (!__builtin_islessequal(a, 1.0L)) return __builtin_isnan(x) ? x + x : __math_invalidl(x);
	return x_fpatan(x_fsqrt((1.0L - x) * (1.0L + x)), x);
}

/* ---------------------------------------------------------- hyperbolic */

long double sinhl(long double x)
{
	long double a = fabsl(x), r;
	if (!__builtin_isfinite(x)) return x + x;
	if (a < 0x1p-32L) return x;
	if (a < 11355.0L) {
		long double e = expm1l(a);
		r = 0.5L * (e + e / (e + 1.0L));
	} else {
		long double e = expl(0.5L * a);
		r = __math_rangel((0.5L * e) * e);
	}
	return x < 0 ? -r : r;
}

long double coshl(long double x)
{
	long double a = fabsl(x);
	if (!__builtin_isfinite(x)) return x * x;
	if (a < 0x1p-33L) return 1.0L;
	if (a < 0.5L) {
		long double e = expm1l(a);
		return 1.0L + (e * e) / (2.0L * (e + 1.0L));
	}
	if (a < 11355.0L) {
		long double e = expl(a);
		return 0.5L * (e + 1.0L / e);
	}
	long double e = expl(0.5L * a);
	return __math_rangel((0.5L * e) * e);
}

long double tanhl(long double x)
{
	long double a = fabsl(x), r;
	if (__builtin_isnan(x)) return x + x;
	if (a < 0x1p-33L) return x;
	if (a > 23.0L) {
		r = 1.0L - fp_barrierl(0x1p-16000L);
	} else {
		long double e = expm1l(2.0L * a);
		r = e / (e + 2.0L);
	}
	return x < 0 ? -r : r;
}

long double asinhl(long double x)
{
	long double a = fabsl(x), r;
	if (!__builtin_isfinite(x)) return x + x;
	if (a < 0x1p-32L) return x;
	if (a > 0x1p33L) {
		r = logl(a) + __ln2l[0];
	} else {
		long double a2 = a * a;
		r = log1pl(a + a2 / (1.0L + x_fsqrt(1.0L + a2)));
	}
	return x < 0 ? -r : r;
}

long double acoshl(long double x)
{
	if (!__builtin_isgreaterequal(x, 1.0L)) return __builtin_isnan(x) ? x + x : __math_invalidl(x);
	if (__builtin_isinf(x)) return x;
	if (x > 0x1p33L) return logl(x) + __ln2l[0];
	if (x >= 2.0L) return logl(2.0L * x - 1.0L / (x + x_fsqrt(x * x - 1.0L)));
	long double t = x - 1.0L;
	return log1pl(t + x_fsqrt(2.0L * t + t * t));
}

long double atanhl(long double x)
{
	long double a = fabsl(x);
	if (!__builtin_isless(a, 1.0L)) {
		if (a == 1.0L) return __math_divzerol(__builtin_signbit(x) ? 1 : 0);
		return __builtin_isnan(x) ? x + x : __math_invalidl(x);
	}
	if (a < 0x1p-33L) return x;
	long double r = 0.5L * log1pl(2.0L * a / (1.0L - a));
	return x < 0 ? -r : r;
}

/* ------------------------------------------------------------- misc */

long double cbrtl(long double x)
{
	if (x == 0 || !__builtin_isfinite(x)) return x + x;
	long double a = fabsl(x);
	int e;
	long double m = frexpl(a, &e);          /* a = m 2^e, m in [0.5, 1) */
	int r3 = ((e % 3) + 3) % 3;
	m = scalbnl(m, r3);                      /* m in [0.5, 4) */
	int q = (e - r3) / 3;
	long double y = cbrt((double)m);
	for (int i = 0; i < 2; i++) y = y - (y * y * y - m) / (3.0L * y * y);
	y = scalbnl(y, q);
	return x < 0 ? -y : y;
}

long double hypotl(long double x, long double y)
{
	if (__builtin_isinf(x) || __builtin_isinf(y)) return __builtin_infl();
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	long double a = fabsl(x), b = fabsl(y);
	if (a < b) { long double t = a; a = b; b = t; }
	if (b == 0) return a;
	int ea = ld_exp(a), eb = ld_exp(b);
	if (ea - eb > 65) return a + b;
	int sc = 0;
	if (ea > 0x3fff + 8000) sc = -9000;
	else if (eb < 0x3fff - 8000) sc = 9000;
	a = scalbnl(a, sc);
	b = scalbnl(b, sc);
	ldd_t s = l_add(l_two_prod(a, a), l_two_prod(b, b));
	long double r = x_fsqrt(s.hi);
	r += (s.hi - r * r + s.lo) / (2.0L * r);
	return scalbnl(r, -sc);
}

/* ------------------------------------------ double-precision evaluated */

long double erfl(long double x) { return erf((double)x); }
long double erfcl(long double x) { return erfc((double)x); }
long double lgammal_r(long double x, int *sg) { return __lgamma_r((double)x, sg); }
long double lgammal(long double x) { return __lgamma_r((double)x, &signgam); }
long double gammal(long double x) { return lgammal(x); }

long double tgammal(long double x)
{
	if (x > 171.0L && x < 1755.6L) {
		/* beyond double range: Gamma(x) = Gamma(x - n) prod (x - i) */
		long double p = 1.0L;
		while (x > 171.0L) {
			x -= 1.0L;
			p *= x;
		}
		return __math_rangel(p * (long double)tgamma((double)x));
	}
	if (x >= 1755.6L) return __math_oflowl(0);
	return tgamma((double)x);
}

long double j0l(long double x) { return j0((double)x); }
long double j1l(long double x) { return j1((double)x); }
long double jnl(int n, long double x) { return jn(n, (double)x); }
long double y0l(long double x) { return y0((double)x); }
long double y1l(long double x) { return y1((double)x); }
long double ynl(int n, long double x) { return yn(n, (double)x); }
