/*
 * lib-spfxd — long double functions for IEEE binary128 (LDBL_MANT_DIG
 * 113: AArch64 and other 64-bit RISC ABIs).  The x87 80-bit format has
 * its own implementation in arch/x86_64/src/math/.
 *
 * There is no binary128 hardware: every long double operation below is a
 * libgcc soft-float call (__addtf3, __multf3, ...), correctly rounded in
 * the current FPCR rounding mode and raising the IEEE flags.  The design
 * follows from that:
 *
 *  - Exact operations work on the bit pattern with 128-bit integers:
 *    rounding to integers, frexp/scalbn/nextafter, fmod/remquo (integer
 *    long division), sqrt (digit-by-digit, rounded in the current mode)
 *    and fma (a 256-bit exact accumulator).
 *  - Argument reduction modulo pi/2 is always Payne-Hanek on integers: the
 *    113-bit significand times a 448-bit window of 2/pi.
 *  - Elementary functions use table-driven reduction and short Taylor
 *    series, with the leading terms carried as double-binary128 ("qd")
 *    pairs.  Tables and coefficients come from tools/gen-ldbl128.py.
 *    The typical error is below 1 ulp; the bounds the test-suite enforces
 *    are listed in tests/math/ulp_check.py.
 *
 * erfl, erfcl, lgammal, tgammal and the Bessel functions are evaluated by
 * the double implementations (about 53 correct bits; see README).
 */
#include <float.h>

#if LDBL_MANT_DIG == 113

#include <errno.h>
#include <fenv.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include "fp.h"

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "ldbl128.c assumes a little-endian binary128 layout"
#endif

#include "ldbl128_data.h"

typedef unsigned __int128 u128;
typedef struct { long double hi, lo; } qd_t;

#define MANT_MASK  ((((u128)1) << 112) - 1)
#define IMPLICIT   (((u128)1) << 112)
#define SIGN_BIT   (((u128)1) << 127)
#define EXP_OF(i)  ((int)((i) >> 112) & 0x7fff)

static always_inline u128 bitsq(long double x)
{
	union { long double f; u128 i; } u = { x };
	return u.i;
}

static always_inline long double asq(u128 i)
{
	union { u128 i; long double f; } u = { i };
	return u.f;
}

/* exact 2^n for -16382 <= n <= 16383 */
static always_inline long double pow2q(int n)
{
	return asq((u128)(0x3fff + n) << 112);
}

static always_inline int clz128(u128 x)
{
	uint64_t hi = (uint64_t)(x >> 64);
	return hi ? __builtin_clzll(hi) : 64 + __builtin_clzll((uint64_t)x);
}

/* ------------------------------------------------- double-binary128 */

static always_inline qd_t q_two_sum(long double a, long double b)
{
	long double s = a + b, bb = s - a;
	return (qd_t){ s, (a - (s - bb)) + (b - bb) };
}

static always_inline qd_t q_fast_two_sum(long double a, long double b)
{
	long double s = a + b;
	return (qd_t){ s, b - (s - a) };
}

/* Dekker's product; |a|, |b| < 2^16300 */
static always_inline qd_t q_two_prod(long double a, long double b)
{
	const long double split = 0x1p57L + 1.0L;
	long double p = a * b;
	long double ca = split * a, cb = split * b;
	long double ah = ca - (ca - a), al = a - ah;
	long double bh = cb - (cb - b), bl = b - bh;
	return (qd_t){ p, ((ah * bh - p) + ah * bl + al * bh) + al * bl };
}

static always_inline qd_t qd_add(qd_t a, qd_t b)
{
	qd_t s = q_two_sum(a.hi, b.hi);
	s.lo += a.lo + b.lo;
	return q_fast_two_sum(s.hi, s.lo);
}

static always_inline qd_t qd_neg(qd_t a)
{
	return (qd_t){ -a.hi, -a.lo };
}

static always_inline qd_t qd_mul(qd_t a, qd_t b)
{
	qd_t p = q_two_prod(a.hi, b.hi);
	p.lo += a.hi * b.lo + a.lo * b.hi;
	return q_fast_two_sum(p.hi, p.lo);
}

static always_inline qd_t qd_mul_c(qd_t a, const long double *c)
{
	return qd_mul(a, (qd_t){ c[0], c[1] });
}

static always_inline qd_t qd_div(qd_t n, qd_t d)
{
	long double q = n.hi / d.hi;
	qd_t p = q_two_prod(q, d.hi);
	long double e = (((n.hi - p.hi) - p.lo) + n.lo - q * d.lo) / d.hi;
	return q_fast_two_sum(q, e);
}

static always_inline qd_t qd_sqrt(qd_t a)
{
	if (a.hi <= 0) return (qd_t){ 0.0L, 0.0L };
	long double s = sqrtl(a.hi);
	qd_t p = q_two_prod(s, s);
	return q_fast_two_sum(s, (((a.hi - p.hi) - p.lo) + a.lo) / (2.0L * s));
}

static always_inline qd_t qd_c(const long double *c)
{
	return (qd_t){ c[0], c[1] };
}

/* ----------------------------------------------------------- manipulation */

long double (fabsl)(long double x)
{
	return asq(bitsq(x) & ~SIGN_BIT);
}

long double (copysignl)(long double x, long double y)
{
	return asq((bitsq(x) & ~SIGN_BIT) | (bitsq(y) & SIGN_BIT));
}

long double frexpl(long double x, int *e)
{
	u128 i = bitsq(x);
	int ee = EXP_OF(i);
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
	return asq((i & ~((u128)0x7fff << 112)) | ((u128)0x3ffe << 112));
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
		/* scale in steps that stay normal, so the result rounds once */
		x *= 0x1p-16382L * 0x1p113L;
		n += 16382 - 113;
		if (n < -16382) {
			x *= 0x1p-16382L * 0x1p113L;
			n += 16382 - 113;
			if (n < -16382) n = -16382;
		}
	}
	long double y = x * pow2q(n);
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
	u128 i = bitsq(x);
	int e = EXP_OF(i);
	if (!e) {
		u128 m = i & MANT_MASK;
		if (!m) {
			fp_force_eval(fp_barrier(0.0) / 0.0);
			errno = EDOM;
			return FP_ILOGB0;
		}
		return (127 - clz128(m)) - 16494;
	}
	if (e == 0x7fff) {
		fp_force_eval(fp_barrier(0.0) / 0.0);
		errno = EDOM;
		return (i & MANT_MASK) ? FP_ILOGBNAN : INT_MAX;
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
	u128 i = bitsq(x);
	if (x == 0) i = (bitsq(y) & SIGN_BIT) | 1;
	else if ((x < y) == !(i >> 127)) i++;   /* away from zero */
	else i--;                                /* toward zero */
	long double r = asq(i);
	int e = EXP_OF(i);
	if (e == 0x7fff) {
		fp_force_evall(x + x);
		errno = ERANGE;
	} else if (!e) {
		fp_force_evall(x * x + r * r);
		errno = ERANGE;
	}
	return r;
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
int __signbitl(long double x) { return (int)(bitsq(x) >> 127); }
int __fpclassifyl(long double x) { return fpclassify(x); }

/* ------------------------------------------------- integral rounding */

/* Integral values: unbiased exponent e >= 112, or the fraction mask is
 * the low 112 - e bits.  floorl/ceill/truncl/roundl raise no flags. */

long double floorl(long double x)
{
	u128 i = bitsq(x);
	int e = EXP_OF(i) - 0x3fff;
	int neg = (int)(i >> 127);
	if (e >= 112) return e == 0x4000 ? x + x : x;
	if (e < 0) {
		if (!(i << 1)) return x;
		return neg ? -1.0L : 0.0L;
	}
	u128 m = MANT_MASK >> e;
	if (!(i & m)) return x;
	i &= ~m;
	if (neg) return asq(i) - 1.0L;       /* exact: integral, |x| < 2^112 */
	return asq(i);
}

long double ceill(long double x)
{
	u128 i = bitsq(x);
	int e = EXP_OF(i) - 0x3fff;
	int neg = (int)(i >> 127);
	if (e >= 112) return e == 0x4000 ? x + x : x;
	if (e < 0) {
		if (!(i << 1)) return x;
		return neg ? -0.0L : 1.0L;
	}
	u128 m = MANT_MASK >> e;
	if (!(i & m)) return x;
	i &= ~m;
	if (!neg) return asq(i) + 1.0L;
	return asq(i);
}

long double truncl(long double x)
{
	u128 i = bitsq(x);
	int e = EXP_OF(i) - 0x3fff;
	if (e >= 112) return e == 0x4000 ? x + x : x;
	if (e < 0) return asq(i & SIGN_BIT);
	return asq(i & ~(MANT_MASK >> e));
}

long double roundl(long double x)
{
	u128 i = bitsq(x);
	int e = EXP_OF(i) - 0x3fff;
	int neg = (int)(i >> 127);
	if (e >= 112) return e == 0x4000 ? x + x : x;
	if (e < 0) {
		if (e == -1) return neg ? -1.0L : 1.0L;
		return asq(i & SIGN_BIT);
	}
	u128 m = MANT_MASK >> e;
	if (!(i & m)) return x;
	int up = (int)((i >> (111 - e)) & 1);   /* the 1/2 bit */
	i &= ~m;
	if (up) return neg ? asq(i) - 1.0L : asq(i) + 1.0L;
	return asq(i);
}

long double roundevenl(long double x)
{
	long double r = roundl(x);
	if (fabsl(r - x) == 0.5L) r = 2.0L * roundl(x * 0.5L);
	return r;
}

/* Round in the current mode: adding and subtracting 2^112 leaves an
 * integer, rounded by the soft-float adder exactly as the mode says. */
long double rintl(long double x)
{
	u128 i = bitsq(x);
	int e = EXP_OF(i) - 0x3fff;
	if (e >= 112) return e == 0x4000 ? x + x : x;
	const long double big = 0x1p112L;
	long double y = (i >> 127) ? fp_barrierl(x - big) + big : fp_barrierl(x + big) - big;
	return copysignl(y, x);
}

long double nearbyintl(long double x)
{
	int saved = fetestexcept(FE_INEXACT);
	long double y = rintl(x);
	if (!saved) feclearexcept(FE_INEXACT);
	return y;
}

static long to_long(long double r, long double x)
{
	if (!(r >= -0x1p63L && r < 0x1p63L)) {
		feraiseexcept(FE_INVALID);
		errno = EDOM;
		return __builtin_signbit(x) ? LONG_MIN : LONG_MAX;
	}
	return (long)r;
}

long lrintl(long double x) { return to_long(rintl(x), x); }
long long llrintl(long double x) { return to_long(rintl(x), x); }
long lroundl(long double x) { return to_long(roundl(x), x); }
long long llroundl(long double x) { return to_long(roundl(x), x); }

/* ------------------------------------------------------- square root */

/* Correctly rounded in the current mode: the 115-bit integer square
 * root of the significand (two bits at a time, with an exact remainder)
 * gives the result, a round bit and a guard bit; the remainder is the
 * sticky bit. */
long double sqrtl(long double x)
{
	u128 i = bitsq(x);
	int be = EXP_OF(i);
	if (be == 0x7fff) {
		if ((i & MANT_MASK) || !(i >> 127)) return x + x;   /* NaN, +inf */
		return __math_invalidl(x);
	}
	if (!(i << 1)) return x;                                 /* +-0 */
	if (i >> 127) return __math_invalidl(x);
	u128 m = i & MANT_MASK;
	int E;                                   /* x = m 2^E, bit 112 set */
	if (be) {
		m |= IMPLICIT;
		E = be - 0x3fff - 112;
	} else {
		int sh = clz128(m) - 15;
		m <<= sh;
		E = 1 - 0x3fff - 112 - sh;
	}
	if (E & 1) {
		m <<= 1;
		E--;
	}
	/* root of M = m 2^116, a 230-bit number: 115 bit pairs, the low 58
	 * of them zero */
	u128 root = 0, rem = 0;
	for (int k = 112; k >= -116; k -= 2) {
		unsigned pair = k >= 0 ? (unsigned)(m >> k) & 3 : 0;
		rem = (rem << 2) | pair;
		u128 trial = (root << 2) | 1;
		root <<= 1;
		if (rem >= trial) {
			rem -= trial;
			root |= 1;
		}
	}
	u128 mant = root >> 2;                   /* 113 bits */
	unsigned rb = (unsigned)root & 3;
	int sticky = rem != 0;
	int E2 = E / 2 - 56;                     /* result = mant 2^E2 */
	if (rb || sticky) {
		int up;
		switch (fegetround()) {
		case FE_UPWARD: up = 1; break;
		case FE_DOWNWARD:
		case FE_TOWARDZERO: up = 0; break;
		default: up = (rb & 2) && ((rb & 1) || sticky || (mant & 1)); break;
		}
		mant += (u128)up;
		feraiseexcept(FE_INEXACT);
	}
	/* mant may have carried to 2^113: the exponent field absorbs it */
	return asq(((u128)(E2 + 112 + 0x3fff - 1) << 112) + mant);
}

/* --------------------------------------------------------------- fma */

struct u256 { u128 hi, lo; };

static int clz256(struct u256 v)
{
	return v.hi ? clz128(v.hi) : (v.lo ? 128 + clz128(v.lo) : 256);
}

static struct u256 shl256(struct u256 v, int n)
{
	if (n <= 0) return v;
	if (n >= 256) return (struct u256){ 0, 0 };
	if (n >= 128) return (struct u256){ v.lo << (n - 128), 0 };
	return (struct u256){ (v.hi << n) | (v.lo >> (128 - n)), v.lo << n };
}

/* shift right, ORing every lost bit into bit 0 */
static struct u256 shr256_sticky(struct u256 v, int n)
{
	if (n <= 0) return v;
	if (n >= 256) return (struct u256){ 0, (v.hi | v.lo) != 0 };
	struct u256 r;
	u128 lost;
	if (n >= 128) {
		r.hi = 0;
		r.lo = n == 128 ? v.hi : v.hi >> (n - 128);
		lost = v.lo | (n == 128 ? 0 : v.hi << (256 - n));
	} else {
		r.hi = v.hi >> n;
		r.lo = (v.lo >> n) | (v.hi << (128 - n));
		lost = v.lo << (128 - n);
	}
	r.lo |= lost != 0;
	return r;
}

/* x = s m 2^e with bit 112 of m set (subnormals normalized) */
static void unpack(long double x, int *s, u128 *m, int *e)
{
	u128 i = bitsq(x);
	int be = EXP_OF(i);
	*s = (int)(i >> 127);
	*m = i & MANT_MASK;
	if (be) {
		*m |= IMPLICIT;
		*e = be - 0x3fff - 112;
	} else {
		int sh = clz128(*m) - 15;
		*m <<= sh;
		*e = 1 - 0x3fff - 112 - sh;
	}
}

long double fmal(long double x, long double y, long double z)
{
	if (!__builtin_isfinite(x) || !__builtin_isfinite(y) || !__builtin_isfinite(z) || x == 0 || y == 0)
		return x * y + z;
	int sx, sy, sz, ex, ey, ez;
	u128 mx, my, mz;
	unpack(x, &sx, &mx, &ex);
	unpack(y, &sy, &my, &ey);
	int sp = sx ^ sy;

	/* exact 226-bit product */
	uint64_t xl = (uint64_t)mx, xh = (uint64_t)(mx >> 64);
	uint64_t yl = (uint64_t)my, yh = (uint64_t)(my >> 64);
	u128 ll = (u128)xl * yl, lh = (u128)xl * yh, hl = (u128)xh * yl, hh = (u128)xh * yh;
	u128 mid = lh + hl;                       /* < 2^114: no overflow */
	struct u256 P;
	P.lo = ll + (mid << 64);
	P.hi = hh + (mid >> 64) + (P.lo < ll);
	int lp = clz256(P);
	P = shl256(P, lp - 1);                    /* MSB at bit 254 */
	int eP = ex + ey - (lp - 1);              /* value = P 2^eP */

	struct u256 R;
	int eR, sR;
	if (z == 0) {
		R = P;
		eR = eP;
		sR = sp;
	} else {
		unpack(z, &sz, &mz, &ez);
		struct u256 Z = shl256((struct u256){ 0, mz }, 254 - 112);
		ez -= 254 - 112;
		/* align to the larger exponent */
		if (eP >= ez) {
			Z = shr256_sticky(Z, eP - ez);
			eR = eP;
		} else {
			P = shr256_sticky(P, ez - eP);
			eR = ez;
		}
		if (sp == sz) {
			R.lo = P.lo + Z.lo;
			R.hi = P.hi + Z.hi + (R.lo < P.lo);
			sR = sp;
		} else {
			int pbig = P.hi > Z.hi || (P.hi == Z.hi && P.lo >= Z.lo);
			struct u256 A = pbig ? P : Z, B = pbig ? Z : P;
			R.lo = A.lo - B.lo;
			R.hi = A.hi - B.hi - (A.lo < B.lo);
			sR = pbig ? sp : sz;
			if (!R.hi && !R.lo) {
				/* exact cancellation */
				return fegetround() == FE_DOWNWARD ? -0.0L : 0.0L;
			}
		}
	}
	/* normalize to MSB at bit 255 */
	int lz = clz256(R);
	R = shl256(R, lz);
	eR -= lz;
	int E = eR + 255;                         /* unbiased exponent */
	int sh = 143;                             /* bits below the 113 kept */
	int tiny = E < -16382;
	if (tiny) sh += -16382 - E;
	u128 mant;
	int half, rest;
	if (sh >= 256) {
		mant = 0;
		half = sh == 256;                 /* the MSB is the half bit */
		rest = sh == 256 ? ((R.hi << 1) || R.lo) : 1;
	} else {
		/* keep the top 256 - sh <= 113 bits */
		mant = sh >= 128 ? R.hi >> (sh - 128) : (R.hi << (128 - sh)) | (R.lo >> sh);
		struct u256 below = shl256(R, 256 - sh);   /* the discarded bits, top-aligned */
		half = (int)(below.hi >> 127);
		rest = (below.hi << 1) || below.lo;
	}
	int inexact = half || rest;
	if (inexact) {
		int up;
		switch (fegetround()) {
		case FE_UPWARD: up = !sR; break;
		case FE_DOWNWARD: up = sR; break;
		case FE_TOWARDZERO: up = 0; break;
		default: up = half && (rest || (mant & 1)); break;
		}
		mant += (u128)up;
	}
	u128 bits;
	if (tiny) {
		bits = mant;                      /* may round up into the normal range */
	} else {
		if (E > 16383 || (E == 16383 && mant >> 113)) {
			feraiseexcept(FE_OVERFLOW | FE_INEXACT);
			errno = ERANGE;
			int mode = fegetround();
			int toinf = mode == FE_TONEAREST || (mode == FE_UPWARD && !sR) || (mode == FE_DOWNWARD && sR);
			long double big = toinf ? __builtin_infl() : LDBL_MAX;
			return sR ? -big : big;
		}
		bits = ((u128)(E + 0x3fff - 1) << 112) + mant;
	}
	if (inexact) {
		feraiseexcept(tiny ? FE_UNDERFLOW | FE_INEXACT : FE_INEXACT);
		if (tiny) errno = ERANGE;
	}
	return asq(bits | ((u128)sR << 127));
}

/* --------------------------------------------------------- remainders */

/* |x| mod |y| by integer long division: returns the remainder as a
 * long double (exact) and the low bits of the quotient in *q. */
static long double rem_core(long double ax, long double ay, unsigned *q)
{
	int s, ex, ey;
	u128 mx, my;
	unpack(ax, &s, &mx, &ex);
	unpack(ay, &s, &my, &ey);
	unsigned qq = 0;
	u128 r = mx;
	if (r >= my) {
		r -= my;
		qq = 1;
	}
	/* 14 quotient bits per step: r < my < 2^113, so r << 14 fits */
	int d = ex - ey;
	while (d > 0) {
		int k = d < 14 ? d : 14;
		r <<= k;
		u128 t = r / my;
		r -= t * my;
		qq = (qq << k) | (unsigned)t;
		d -= k;
	}
	*q = qq;
	if (!r) return 0.0L;
	/* value r 2^ey: exact, normal or subnormal */
	int sh = clz128(r) - 15;
	r <<= sh;
	int e = ey - sh + 112;                   /* unbiased exponent */
	if (e >= -16382) return asq(((u128)(e + 0x3fff) << 112) | (r & MANT_MASK));
	return asq(r >> (-16382 - e));
}

long double fmodl(long double x, long double y)
{
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	if (__builtin_isinf(x) || y == 0) return __math_invalidl(x * y);
	if (__builtin_isinf(y)) return x;
	long double ax = fabsl(x), ay = fabsl(y);
	if (ax < ay) return x;
	unsigned q;
	return copysignl(rem_core(ax, ay, &q), x);
}

long double remquol(long double x, long double y, int *quo)
{
	*quo = 0;
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	if (__builtin_isinf(x) || y == 0) return __math_invalidl(x * y);
	if (__builtin_isinf(y)) return x;
	long double ax = fabsl(x), ay = fabsl(y), r;
	unsigned q = 0;
	if (ax < ay) r = ax;
	else r = rem_core(ax, ay, &q);
	/* round the quotient to nearest, ties to even; r - ay is exact */
	long double r2 = 2.0L * r;
	if (r2 > ay || (r2 == ay && (q & 1))) {
		r -= ay;
		q++;
	}
	int qs = (int)(q & 7);
	*quo = __builtin_signbit(x) != __builtin_signbit(y) ? -qs : qs;
	return __builtin_signbit(x) ? -r : r;
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

static always_inline long double nearest(long double z)
{
	return roundl(z);
}

/* e^r - 1 for |r| <= ln(2)/64 (directed modes may leave |r| up to twice
 * that); the omitted terms are below 2^-118 relative */
static long double expm1_poly(long double r)
{
	long double p = EXP_C[12];
	for (int k = 11; k >= 0; k--) p = EXP_C[k] + r * p;
	return r + r * r * p;
}

/* exp(hi + lo) 2^sc, |lo| <= 2^-100 |hi|, |hi| < 12000; sets errno on
 * overflow and underflow */
static long double exp_core(long double hi, long double lo, int sc)
{
	long double nf = nearest(hi * INV_LN2_64);
	int n = (int)nf;
	long double r = ((hi - nf * LN2_64[0]) - nf * LN2_64[1]) + lo;
	int j = n & 63, m = (n - j) / 64;
	long double p = expm1_poly(r);
	long double t0 = EXP2_T[j][0], t1 = EXP2_T[j][1];
	long double y = t0 + (t1 + t0 * p);
	return __math_rangel(scalbnl(y, m + sc));
}

long double expl(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (x > 11357.0L) return __math_oflowl(0);
	if (x < -11500.0L) return __math_uflowl(0);
	if (fabsl(x) < 0x1p-115L) return 1.0L + x;
	return exp_core(x, 0.0L, 0);
}

long double exp2l(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (x >= 16384.0L) return __math_oflowl(0);
	if (x < -16495.0L) return __math_uflowl(0);
	long double nf = nearest(64.0L * x);
	long double d = x - nf * (1.0L / 64);    /* exact */
	if (d == 0) {
		/* 2^(n/64): a table entry, exactly rounded */
		int n = (int)nf, j = n & 63;
		return __math_rangel(scalbnl(EXP2_T[j][0] + EXP2_T[j][1], (n - j) / 64));
	}
	qd_t r = qd_mul_c((qd_t){ d, 0.0L }, LN2);
	int n = (int)nf, j = n & 63;
	long double p = expm1_poly(r.hi + r.lo);
	long double t0 = EXP2_T[j][0], t1 = EXP2_T[j][1];
	return __math_rangel(scalbnl(t0 + (t1 + t0 * p), (n - j) / 64));
}

long double exp10l(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (x > 4933.0L) return __math_oflowl(0);
	if (x < -4967.0L) return __math_uflowl(0);
	/* exact for the integers whose powers of ten are representable */
	if (x == truncl(x) && fabsl(x) <= 48.0L) {
		long double p = 1.0L, b = 10.0L;
		int n = (int)fabsl(x);
		for (;;) {                   /* every factor 10^(2^i) <= 10^32 is exact */
			if (n & 1) p *= b;
			if (!(n >>= 1)) break;
			b *= b;
		}
		return x < 0 ? 1.0L / p : p;
	}
	qd_t a = qd_mul_c((qd_t){ x, 0.0L }, LN10);
	return exp_core(a.hi, a.lo, 0);
}

long double pow10l(long double x)
{
	return exp10l(x);
}

long double expm1l(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (x > 11357.0L) return __math_oflowl(0);
	if (x < -80.0L) return fp_barrierl(-1.0L) + 0x1p-200L;   /* e^x < 2^-115 */
	if (fabsl(x) < 0x1p-114L) {
		if (x != 0 && fabsl(x) < LDBL_MIN) errno = ERANGE;
		return x;
	}
	long double nf = nearest(x * INV_LN2_64);
	int n = (int)nf;
	long double r = (x - nf * LN2_64[0]) - nf * LN2_64[1];
	int j = n & 63, m = (n - j) / 64;
	long double p = expm1_poly(r);
	long double t0 = EXP2_T[j][0], t1 = EXP2_T[j][1];
	if (m > 120) return __math_rangel(scalbnl(t0 + (t1 + t0 * p), m));
	/* 2^m (t0 + t1 + t0 p) - 1, with 2^m t0 - 1 formed exactly */
	long double sc = pow2q(m);
	qd_t s = q_two_sum(sc * t0, -1.0L);
	long double tail = sc * (t1 + t0 * p);
	return s.hi + (s.lo + tail);
}

/* --------------------------------------------------------- logarithms */

/* x = 2^k (1 + j/128) (1 + f/F): returns ln(m/F) + ln(F) as a qd with
 * the integer k separately.  x finite, > 0. */
static qd_t log_parts(long double x, int *kk)
{
	u128 i = bitsq(x);
	int k = 0;
	if (!EXP_OF(i)) {
		x *= 0x1p120L;
		i = bitsq(x);
		k = -120;
	}
	k += EXP_OF(i) - 0x3fff;
	long double m = asq((i & MANT_MASK) | ((u128)0x3fff << 112));   /* [1, 2) */
	int j = (int)nearest((m - 1.0L) * 128.0L);
	if (j == 128) {
		/* m just below 2: use m/2 just below 1 instead */
		m *= 0.5L;
		k++;
		j = 0;
	}
	long double F = 1.0L + j * (1.0L / 128);
	long double f = m - F;                   /* exact, |f| <= 1/256 */
	/* ln(m/F) = 2 atanh(s), s = f / (2F + f), carried as a qd */
	qd_t den = q_two_sum(2.0L * F, f);
	qd_t s = qd_div((qd_t){ f, 0.0L }, den);
	long double z = s.hi * s.hi;
	long double p = ATANH_C[6];
	for (int c = 5; c >= 0; c--) p = ATANH_C[c] + z * p;
	long double t = 2.0L * s.hi * z * p;
	qd_t a = q_two_sum(LOG_T[j][0], 2.0L * s.hi);
	a.lo += LOG_T[j][1] + 2.0L * s.lo + t;
	*kk = k;
	return q_fast_two_sum(a.hi, a.lo);
}

/* ln x as a qd, ~2^-220 absolute plus 2^-128 relative */
static qd_t log_dd(long double x)
{
	int k;
	qd_t l = log_parts(x, &k);
	long double kf = (long double)k;
	qd_t a = q_two_sum(kf * LN2[0], l.hi);      /* k*LN2[0] is exact */
	a.lo += kf * LN2[1] + l.lo;
	return q_fast_two_sum(a.hi, a.lo);
}

/* the special cases shared by the logarithms; returns 1 when *r is set */
static int log_special(long double x, long double *r)
{
	if (__builtin_isnan(x)) { *r = x + x; return 1; }
	if (x == 0) { *r = __math_divzerol(1); return 1; }
	if (x < 0) { *r = __math_invalidl(x); return 1; }
	if (__builtin_isinf(x)) { *r = x; return 1; }
	return 0;
}

long double logl(long double x)
{
	long double r;
	if (log_special(x, &r)) return r;
	return log_dd(x).hi;
}

long double log2l(long double x)
{
	long double r;
	if (log_special(x, &r)) return r;
	int k;
	qd_t l = qd_mul_c(log_parts(x, &k), LOG2E);
	qd_t a = q_two_sum((long double)k, l.hi);
	return a.hi + (a.lo + l.lo);
}

long double log10l(long double x)
{
	long double r;
	if (log_special(x, &r)) return r;
	int k;
	qd_t l = qd_mul_c(log_parts(x, &k), LOG10E);
	qd_t kl = qd_mul_c((qd_t){ (long double)k, 0.0L }, LOG10_2);
	qd_t a = qd_add(kl, l);
	return a.hi;
}

long double log1pl(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	if (x == -1.0L) return __math_divzerol(1);
	if (x < -1.0L) return __math_invalidl(x);
	if (__builtin_isinf(x)) return x;
	if (fabsl(x) < 0x1p-114L) {
		if (x != 0 && fabsl(x) < LDBL_MIN) errno = ERANGE;
		return x;
	}
	/* ln(u + c) = ln u + c/u with u + c = 1 + x exactly */
	qd_t u = q_two_sum(1.0L, x);
	if (u.hi == 0) return __math_divzerol(1);
	qd_t l = log_dd(u.hi);
	return l.hi + (l.lo + u.lo / u.hi);
}

/* ------------------------------------------------------------------ pow */

/* 0: not an integer, 1: odd integer, 2: even integer (y finite) */
static int int_kindl(long double y)
{
	if (truncl(y) != y) return 0;
	if (fabsl(y) >= 0x1p113L) return 2;
	return fmodl(y, 2.0L) != 0 ? 1 : 2;
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
	if (fabsl(y) >= 0x1p130L) {
		if ((x > 1.0L) == (y > 0)) return __math_oflowl((uint32_t)sign);
		return __math_uflowl((uint32_t)sign);
	}
	if (y == 1.0L) return sign ? -x : x;
	if (y == 2.0L) return __math_rangel(x * x);
	if (y == 0.5L) return sqrtl(x);
	qd_t l = log_dd(x);
	qd_t e = q_two_prod(y, l.hi);
	e.lo += y * l.lo;
	e = q_fast_two_sum(e.hi, e.lo);
	if (e.hi > 11357.0L) return __math_oflowl((uint32_t)sign);
	if (e.hi < -11500.0L) return __math_uflowl((uint32_t)sign);
	long double r = exp_core(e.hi, e.lo, 0);
	return sign ? -r : r;
}

/* ------------------------------------------------------------- trig */

/* 64 bits of 2/pi starting at bit j (1-indexed after the binary point) */
static uint64_t pi_bits(int j)
{
	int idx = (j - 1) >> 6, off = (j - 1) & 63;
	uint64_t w = __two_over_pi_bits[idx] << off;
	if (off) w |= __two_over_pi_bits[idx + 1] >> (64 - off);
	return w;
}

#define PW 7                  /* window: 448 bits of 2/pi */
#define PN (PW + 2)           /* product words */

/* bits [pos, pos + cnt) of the little-endian word array p, cnt <= 64 */
static uint64_t get_bits(const uint64_t *p, int pos, int cnt)
{
	uint64_t w = 0;
	for (int b = 0; b < cnt; b++) {
		int q = pos + b;
		if (q >= 0 && q < 64 * PN && ((p[q >> 6] >> (q & 63)) & 1)) w |= 1ULL << b;
	}
	return w;
}

/*
 * Payne-Hanek: |x| = m 2^E, m < 2^113.  Bits of 2/pi before bit E-1
 * contribute multiples of 4 to x 2/pi and are skipped; the 448-bit window
 * leaves a truncation error below 2^-330 in the fraction, far below the
 * worst cancellation of a binary128 argument.  Returns n mod 4 and
 * r = x - n pi/2 as a qd.
 */
static int rem_pio2q(long double x, qd_t *r)
{
	int neg, E;
	u128 m;
	unpack(x, &neg, &m, &E);
	int i0 = E - 1 > 1 ? E - 1 : 1;
	uint64_t B[PW], P[PN] = { 0 };
	for (int k = 0; k < PW; k++) B[PW - 1 - k] = pi_bits(i0 + 64 * k);
	uint64_t M[2] = { (uint64_t)m, (uint64_t)(m >> 64) };
	for (int a = 0; a < 2; a++) {
		uint64_t carry = 0;
		for (int b = 0; b < PW; b++) {
			u128 t = (u128)M[a] * B[b] + P[a + b] + carry;
			P[a + b] = (uint64_t)t;
			carry = (uint64_t)(t >> 64);
		}
		for (int c = a + PW; carry && c < PN; c++) {
			u128 t = (u128)P[c] + carry;
			P[c] = (uint64_t)t;
			carry = (uint64_t)(t >> 64);
		}
	}
	int S = i0 + 64 * PW - 1 - E;            /* fraction bits of P */
	unsigned n = (unsigned)get_bits(P, S, 2);
	int fneg = 0;
	if (get_bits(P, S - 1, 1)) {
		/* fraction >= 1/2: round n up, fraction := 2^S - fraction */
		n++;
		fneg = 1;
		uint64_t c = 1;
		for (int w = 0; w < PN; w++) {
			u128 t = (u128)(~P[w]) + c;
			P[w] = (uint64_t)t;
			c = (uint64_t)(t >> 64);
		}
	}
	/* clear the integer bits */
	for (int b = S; b < 64 * PN; b++) P[b >> 6] &= ~(1ULL << (b & 63));
	int top = -1;
	for (int w = PN - 1; w >= 0 && top < 0; w--)
		if (P[w]) top = 64 * w + 63 - __builtin_clzll(P[w]);
	if (top < 0) {
		*r = (qd_t){ 0.0L, 0.0L };
	} else {
		/* three exact chunks of 100, 100 and 56 bits below the top */
		u128 c0 = ((u128)get_bits(P, top - 35, 36) << 64) | get_bits(P, top - 99, 64);
		u128 c1 = ((u128)get_bits(P, top - 135, 36) << 64) | get_bits(P, top - 199, 64);
		uint64_t c2 = get_bits(P, top - 255, 56);
		int e0 = top - 99 - S;
		long double h = scalbnl((long double)c0, e0);
		long double mid = scalbnl((long double)c1, e0 - 100);
		long double lo = scalbnl((long double)c2, e0 - 156);
		qd_t f = q_fast_two_sum(h, mid);
		f.lo += lo;
		*r = qd_mul_c(f, PIO2);
		if (fneg) *r = qd_neg(*r);
	}
	if (neg) {
		*r = qd_neg(*r);
		n = -n;
	}
	return (int)(n & 3);
}

/* sin(h + l), |h| <= pi/4 + 2^-60; truncation below 2^-120 relative */
static qd_t sin_dd(qd_t r)
{
	long double h = r.hi, z = h * h;
	long double p = SIN_C[13];
	for (int k = 12; k >= 0; k--) p = SIN_C[k] + z * p;
	return q_fast_two_sum(h, h * z * p + r.lo * (1.0L - 0.5L * z));
}

/* cos(h + l) */
static qd_t cos_dd(qd_t r)
{
	long double h = r.hi;
	qd_t z2 = q_two_prod(h, h);
	long double z = z2.hi, hz = 0.5L * z;
	long double w = 1.0L - hz;
	long double p = COS_C[13];
	for (int k = 12; k >= 0; k--) p = COS_C[k] + z * p;
	long double c = (((1.0L - w) - hz) - 0.5L * z2.lo) + (z * z * p - h * r.lo);
	return q_fast_two_sum(w, c);
}

static int trig_reduce(long double x, qd_t *r)
{
	if (fabsl(x) <= 0.785398163397448309615660845819875721L) {
		*r = (qd_t){ x, 0.0L };
		return 0;
	}
	return rem_pio2q(x, r);
}

long double sinl(long double x)
{
	if (!__builtin_isfinite(x)) return __builtin_isnan(x) ? x + x : __math_invalidl(x);
	if (fabsl(x) < 0x1p-57L) {
		if (x != 0 && fabsl(x) < LDBL_MIN) errno = ERANGE;
		return x;
	}
	qd_t r;
	switch (trig_reduce(x, &r)) {
	case 0: return sin_dd(r).hi;
	case 1: return cos_dd(r).hi;
	case 2: return -sin_dd(r).hi;
	default: return -cos_dd(r).hi;
	}
}

long double cosl(long double x)
{
	if (!__builtin_isfinite(x)) return __builtin_isnan(x) ? x + x : __math_invalidl(x);
	if (fabsl(x) < 0x1p-57L) return 1.0L - fabsl(x);
	qd_t r;
	switch (trig_reduce(x, &r)) {
	case 0: return cos_dd(r).hi;
	case 1: return -sin_dd(r).hi;
	case 2: return -cos_dd(r).hi;
	default: return sin_dd(r).hi;
	}
}

void sincosl(long double x, long double *s, long double *c)
{
	if (!__builtin_isfinite(x)) {
		*s = *c = __builtin_isnan(x) ? x + x : __math_invalidl(x);
		return;
	}
	if (fabsl(x) < 0x1p-57L) {
		*s = x;
		*c = 1.0L - fabsl(x);
		return;
	}
	qd_t r;
	int n = trig_reduce(x, &r);
	long double sv = sin_dd(r).hi, cv = cos_dd(r).hi;
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
	if (fabsl(x) < 0x1p-57L) {
		if (x != 0 && fabsl(x) < LDBL_MIN) errno = ERANGE;
		return x;
	}
	qd_t r;
	int n = trig_reduce(x, &r);
	qd_t s = sin_dd(r), c = cos_dd(r);
	if (n & 1) return -qd_div(c, s).hi;
	return qd_div(s, c).hi;
}

/* ---------------------------------------------------- inverse trig */

/* atan(t) for t = th + tl in [0, 1]: atan(c) + atan((t - c)/(1 + t c))
 * with c = j/64 nearest t; |u| <= 1/128, truncation below 2^-120 */
static qd_t atan01(qd_t t)
{
	int j = (int)nearest(t.hi * 64.0L);
	long double c = j * (1.0L / 64);
	qd_t u;
	if (j == 0) {
		u = t;
	} else {
		qd_t num = q_two_sum(t.hi - c, t.lo);  /* t.hi - c exact */
		qd_t p = q_two_prod(t.hi, c);
		qd_t den = q_two_sum(1.0L, p.hi);
		den.lo += p.lo + t.lo * c;
		u = qd_div(num, q_fast_two_sum(den.hi, den.lo));
	}
	long double z = u.hi * u.hi;
	long double q = ATAN_C[8];
	for (int k = 7; k >= 0; k--) q = ATAN_C[k] + z * q;
	qd_t a = q_two_sum(ATAN_T[j][0], u.hi);
	a.lo += ATAN_T[j][1] + u.lo + u.hi * z * q;
	return q_fast_two_sum(a.hi, a.lo);
}

/* atan(y/x) for y, x >= 0, not both zero, within the normal range */
static qd_t atan2_dd(qd_t y, qd_t x)
{
	if (y.hi <= x.hi) return atan01(qd_div(y, x));
	return qd_add(qd_c(PIO2), qd_neg(atan01(qd_div(x, y))));
}

long double atanl(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	long double a = fabsl(x);
	if (a < 0x1p-57L) {
		if (x != 0 && a < LDBL_MIN) errno = ERANGE;
		return x;
	}
	qd_t r;
	if (a <= 1.0L) {
		r = atan01((qd_t){ a, 0.0L });
	} else if (a > 0x1p115L) {
		r = qd_c(PIO2);
	} else {
		r = qd_add(qd_c(PIO2), qd_neg(atan01(qd_div((qd_t){ 1.0L, 0.0L }, (qd_t){ a, 0.0L }))));
	}
	long double v = r.hi + r.lo;
	return x < 0 ? -v : v;
}

long double atan2l(long double y, long double x)
{
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	int sy = __builtin_signbit(y) != 0, sx = __builtin_signbit(x) != 0;
	long double v;
	if (y == 0) {
		if (!sx) return y;
		v = PI[0] + PI[1];
		return sy ? -v : v;
	}
	if (x == 0 || __builtin_isinf(y)) {
		if (__builtin_isinf(x)) {
			/* pi/4 or 3pi/4 */
			qd_t q = { PIO2[0] * 0.5L, PIO2[1] * 0.5L };
			if (sx) q = qd_add(qd_c(PI), qd_neg(q));
			v = q.hi + q.lo;
		} else {
			v = PIO2[0] + PIO2[1];
		}
		return sy ? -v : v;
	}
	if (__builtin_isinf(x)) {
		v = sx ? PI[0] + PI[1] : 0.0L;
		return sy ? -v : v;
	}
	long double ay = fabsl(y), ax = fabsl(x);
	int ky = ilogbl(ay), kx = ilogbl(ax);
	qd_t r;
	if (ky - kx < -120) {
		/* atan(t) = t to within 2^-240 relative */
		long double t = ay / ax;
		if (!sx) {
			if (t < LDBL_MIN) errno = ERANGE;
			return sy ? -t : t;
		}
		r = (qd_t){ t, 0.0L };
	} else if (kx - ky < -120) {
		r = qd_add(qd_c(PIO2), (qd_t){ -(ax / ay), 0.0L });
	} else {
		/* scale the larger to [1, 2): the qd arithmetic stays in range */
		int k = ky > kx ? ky : kx;
		r = atan2_dd((qd_t){ scalbnl(ay, -k), 0.0L }, (qd_t){ scalbnl(ax, -k), 0.0L });
	}
	if (sx) r = qd_add(qd_c(PI), qd_neg(r));
	v = r.hi + r.lo;
	return sy ? -v : v;
}

/* sqrt(1 - a^2) for 0 <= a <= 1 as a qd */
static qd_t sqrt1m(long double a)
{
	qd_t d1 = q_two_sum(1.0L, -a), d2 = q_two_sum(1.0L, a);
	return qd_sqrt(qd_mul(d1, d2));
}

long double asinl(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	long double a = fabsl(x);
	if (a > 1.0L) return __math_invalidl(x);
	if (a < 0x1p-57L) {
		if (x != 0 && a < LDBL_MIN) errno = ERANGE;
		return x;
	}
	qd_t r = atan2_dd((qd_t){ a, 0.0L }, sqrt1m(a));
	long double v = r.hi + r.lo;
	return x < 0 ? -v : v;
}

long double acosl(long double x)
{
	if (__builtin_isnan(x)) return x + x;
	long double a = fabsl(x);
	if (a > 1.0L) return __math_invalidl(x);
	qd_t r;
	if (a == 0) r = qd_c(PIO2);
	else r = atan2_dd(sqrt1m(a), (qd_t){ a, 0.0L });
	if (x < 0) r = qd_add(qd_c(PI), qd_neg(r));
	return r.hi + r.lo;
}

/* ---------------------------------------------------------- hyperbolic */

long double sinhl(long double x)
{
	long double a = fabsl(x), r;
	if (!__builtin_isfinite(x)) return x + x;
	if (a < 0x1p-57L) {
		if (x != 0 && a < LDBL_MIN) errno = ERANGE;
		return x;
	}
	if (a < 80.0L) {
		long double e = expm1l(a);
		r = 0.5L * (e + e / (e + 1.0L));
	} else if (a < 11400.0L) {
		r = exp_core(a, 0.0L, -1);          /* e^-a is below 2^-230 relative */
	} else {
		r = __math_oflowl(0);
	}
	return x < 0 ? -r : r;
}

long double coshl(long double x)
{
	long double a = fabsl(x);
	if (!__builtin_isfinite(x)) return x * x;
	if (a < 0x1p-57L) return 1.0L + a;
	if (a < 0.5L) {
		long double e = expm1l(a);
		return 1.0L + (e * e) / (2.0L * (e + 1.0L));
	}
	if (a < 80.0L) {
		long double e = expl(a);
		return 0.5L * e + 0.5L / e;
	}
	if (a < 11400.0L) return exp_core(a, 0.0L, -1);
	return __math_oflowl(0);
}

long double tanhl(long double x)
{
	long double a = fabsl(x), r;
	if (__builtin_isnan(x)) return x + x;
	if (a < 0x1p-57L) {
		if (x != 0 && a < LDBL_MIN) errno = ERANGE;
		return x;
	}
	if (a > 40.0L) {
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
	if (a < 0x1p-57L) {
		if (x != 0 && a < LDBL_MIN) errno = ERANGE;
		return x;
	}
	if (a > 0x1p57L) {
		qd_t l = qd_add(log_dd(a), qd_c(LN2));
		r = l.hi;
	} else if (a >= 2.0L) {
		r = logl(2.0L * a + 1.0L / (sqrtl(a * a + 1.0L) + a));
	} else {
		long double a2 = a * a;
		r = log1pl(a + a2 / (1.0L + sqrtl(1.0L + a2)));
	}
	return x < 0 ? -r : r;
}

long double acoshl(long double x)
{
	if (!__builtin_isgreaterequal(x, 1.0L)) return __builtin_isnan(x) ? x + x : __math_invalidl(x);
	if (__builtin_isinf(x)) return x;
	if (x > 0x1p57L) return qd_add(log_dd(x), qd_c(LN2)).hi;
	if (x >= 2.0L) return logl(2.0L * x - 1.0L / (x + sqrtl(x * x - 1.0L)));
	long double t = x - 1.0L;
	return log1pl(t + sqrtl(2.0L * t + t * t));
}

long double atanhl(long double x)
{
	long double a = fabsl(x), r;
	if (!__builtin_isless(a, 1.0L)) {
		if (a == 1.0L) return __math_divzerol(__builtin_signbit(x) ? 1 : 0);
		return __builtin_isnan(x) ? x + x : __math_invalidl(x);
	}
	if (a < 0x1p-57L) {
		if (x != 0 && a < LDBL_MIN) errno = ERANGE;
		return x;
	}
	if (a < 0.5L) r = 0.5L * log1pl(2.0L * a + 2.0L * a * a / (1.0L - a));
	else r = 0.5L * log1pl(2.0L * a / (1.0L - a));
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
	y = y - (y * y * y - m) / (3.0L * y * y);
	/* last step with the residual m - y^3 formed exactly */
	qd_t y2 = q_two_prod(y, y);
	qd_t y3 = qd_mul(y2, (qd_t){ y, 0.0L });
	qd_t res = qd_add((qd_t){ m, 0.0L }, qd_neg(y3));
	y = y + res.hi / (3.0L * y2.hi);
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
	int ea = ilogbl(a), eb = ilogbl(b);
	if (ea - eb > 116) return a + b;
	int sc = 0;
	if (ea > 8000) sc = -9000;
	else if (eb < -8000) sc = 9000;
	a = scalbnl(a, sc);
	b = scalbnl(b, sc);
	qd_t s = qd_add(q_two_prod(a, a), q_two_prod(b, b));
	long double r = sqrtl(s.hi);
	qd_t p = q_two_prod(r, r);
	r += (((s.hi - p.hi) - p.lo) + s.lo) / (2.0L * r);
	return __math_rangel(scalbnl(r, -sc));
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

#else
typedef int spfxd_ldbl128_unused;
#endif
