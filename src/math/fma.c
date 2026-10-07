/*
 * lib-spfxd — fused multiply-add: fmaf, fma, fmal.
 *
 * On CPUs with FMA3 the instruction computes fma and fmaf.  Otherwise
 * (and for results that may need errno) x*y + z is computed exactly in
 * integer arithmetic: the product of the significands is exact
 * in 128 bits, the addend is aligned against it in a 256-bit accumulator
 * (bits shifted beyond the accumulator collapse into a sticky bit), and the
 * exact sum is rounded once to the target precision in the current
 * rounding mode, with gradual underflow and the IEEE exceptions.
 */
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include "fp.h"

typedef unsigned __int128 u128;

struct fmt { int prec, emin, emax; };   /* emin/emax: exponent of the leading bit */

/* 256-bit value as hi:lo */
struct u256 { u128 hi, lo; };

static struct u256 shr256(struct u256 v, int d)
{
	if (d <= 0) return v;
	int sticky = 0;
	if (d >= 256) {
		sticky = v.hi || v.lo;
		v.hi = v.lo = 0;
	} else if (d >= 128) {
		sticky = v.lo != 0 || (d > 128 && (v.hi << (256 - d)) != 0);
		v.lo = d == 128 ? v.hi : v.hi >> (d - 128);
		v.hi = 0;
	} else {
		sticky = (v.lo << (128 - d)) != 0;
		v.lo = (v.lo >> d) | (v.hi << (128 - d));
		v.hi >>= d;
	}
	v.lo |= (u128)sticky;
	return v;
}

static int clz256(struct u256 v)
{
	if (v.hi) {
		uint64_t h = (uint64_t)(v.hi >> 64);
		return h ? __builtin_clzll(h) : 64 + __builtin_clzll((uint64_t)v.hi);
	}
	uint64_t h = (uint64_t)(v.lo >> 64);
	return 128 + (h ? __builtin_clzll(h) : 64 + __builtin_clzll((uint64_t)v.lo));
}

/* Bits [pos, pos+n) of v (n <= 64). */
static uint64_t get_bits(struct u256 v, int pos, int n)
{
	u128 w;
	if (pos >= 128) w = v.hi >> (pos - 128);
	else if (pos == 0) w = v.lo;
	else w = (v.lo >> pos) | (v.hi << (128 - pos));
	return n == 64 ? (uint64_t)w : (uint64_t)w & ((1ULL << n) - 1);
}

/* Any bit set below position pos? */
static int any_below(struct u256 v, int pos)
{
	if (pos <= 0) return 0;
	if (pos >= 256) return v.hi || v.lo;
	if (pos > 128) return v.lo != 0 || (v.hi << (256 - pos)) != 0;
	if (pos == 128) return v.lo != 0;
	return (v.lo << (128 - pos)) != 0;
}

/*
 * sx, mx, ex: operand x = (-1)^sx mx 2^ex with mx != 0 (likewise y, z;
 * mz may be 0).  Returns the rounded result as a long double, which holds
 * every float/double/extended result exactly.
 */
static long double fma_core(int sx, uint64_t mx, int ex, int sy, uint64_t my, int ey,
                            int sz, uint64_t mz, int ez, struct fmt f)
{
	int sp = sx ^ sy;
	/* product: 128-bit, placed in the high half of a 256-bit accumulator */
	struct u256 P = { (u128)mx * my, 0 };
	int eP = ex + ey - 128;                  /* value = P * 2^eP */
	int ln = clz256(P);
	if (ln) { P.hi <<= ln; eP -= ln; }       /* P.lo == 0: a plain shift */

	struct u256 R;
	int eR, sR;
	if (!mz) {
		R = P;
		eR = eP;
		sR = sp;
	} else {
		/* z normalized to the top of its own accumulator */
		int lz = __builtin_clzll(mz);
		struct u256 Z = { (u128)(mz << lz) << 64, 0 };
		int eZ = ez - lz - 192;
		struct u256 *A = &P, *B = &Z;
		int eA = eP, eB = eZ, sA = sp, sB = sz;
		if (eZ > eP || (eZ == eP && Z.hi > P.hi)) {
			A = &Z; B = &P; eA = eZ; eB = eP; sA = sz; sB = sp;
		}
		/* leave 2 bits of headroom for the carry of an addition */
		struct u256 a = shr256(*A, 2), b = shr256(*B, 2 + (eA - eB));
		eR = eA + 2;
		if (sA == sB) {
			R.lo = a.lo + b.lo;
			R.hi = a.hi + b.hi + (R.lo < a.lo);
		} else {
			R.lo = a.lo - b.lo;
			R.hi = a.hi - b.hi - (a.lo < b.lo);
		}
		sR = sA;
		if (!R.hi && !R.lo) {
			/* exact zero: +0 except when rounding downward */
			return fegetround() == FE_DOWNWARD ? -0.0L : 0.0L;
		}
	}

	/* R * 2^eR, leading bit at t */
	int t = 255 - clz256(R);
	int lead = t + eR;                       /* exponent of the leading bit */
	int keep = f.prec;
	if (lead < f.emin) keep -= f.emin - lead;
	int cut = t - keep + 1;                  /* lowest kept bit position */
	uint64_t q;
	int inexact = 0, up = 0;
	if (cut <= 0) {
		q = get_bits(R, 0, t + 1) << (-cut);   /* exact */
	} else {
		int halfbit, rest;
		if (cut - 1 > t) {
			/* every bit is below the half-ulp position */
			q = 0;
			halfbit = 0;
			rest = 1;
		} else {
			q = cut > t ? 0 : get_bits(R, cut, t - cut + 1);
			halfbit = (int)get_bits(R, cut - 1, 1);
			rest = any_below(R, cut - 1);
		}
		inexact = halfbit || rest;
		if (inexact) {
			switch (fegetround()) {
			case FE_TONEAREST: up = halfbit && (rest || (q & 1)); break;
			case FE_UPWARD: up = !sR; break;
			case FE_DOWNWARD: up = sR; break;
			default: up = 0;
			}
		}
	}
	int escale = cut + eR;                   /* result = q * 2^escale */
	if (up) {
		q++;
		if (keep > 0 && keep < 64 && q == (1ULL << keep)) {
			q >>= 1;
			escale++;
		} else if (keep == 64 && q == 0) {
			q = 1ULL << 63;
			escale++;
		}
	}
	if (q && 63 - __builtin_clzll(q) + escale > f.emax) {
		feraiseexcept(FE_OVERFLOW | FE_INEXACT);
		errno = ERANGE;
		int mode = fegetround();
		long double big;
		int toinf = mode == FE_TONEAREST || (mode == FE_UPWARD && !sR) || (mode == FE_DOWNWARD && sR);
		if (toinf) big = __builtin_infl();
		else if (f.prec == 24) big = FLT_MAX;
		else if (f.prec == 53) big = DBL_MAX;
		else big = LDBL_MAX;
		return sR ? -big : big;
	}
	if (inexact) {
		int tiny = lead < f.emin;
		feraiseexcept(tiny ? FE_UNDERFLOW | FE_INEXACT : FE_INEXACT);
		if (tiny) errno = ERANGE;
	}
	long double r = scalbnl((long double)q, escale);
	return sR ? -r : r;
}

static const struct fmt F32 = { 24, -126, 127 }, F64 = { 53, -1022, 1023 }, F80 = { 64, -16382, 16383 };

double fma(double x, double y, double z)
{
#ifdef __x86_64__
	/* hardware FMA when present: correctly rounded in every mode, with
	 * the IEEE flags; results that may need errno (infinite, NaN, zero,
	 * tiny) take the software path below, which reports them */
	if (likely(__cpu_features & 8 /* CPU_FMA */)) {
		double r = z;
		__asm__ ("vfmadd231sd %2, %1, %0" : "+x"(r) : "x"(x), "x"(y));
		if (likely(__builtin_fabs(r) >= DBL_MIN && __builtin_fabs(r) <= DBL_MAX)) return r;
	}
#endif
	if (!__builtin_isfinite(x) || !__builtin_isfinite(y) || !__builtin_isfinite(z) || x == 0 || y == 0)
		return x * y + z;
	uint64_t ux = asuint64(x), uy = asuint64(y), uz = asuint64(z);
	int ex = (int)(ux >> 52 & 0x7ff), ey = (int)(uy >> 52 & 0x7ff), ez = (int)(uz >> 52 & 0x7ff);
	uint64_t mx = ux & 0x000fffffffffffffULL, my = uy & 0x000fffffffffffffULL, mz = uz & 0x000fffffffffffffULL;
	if (ex) mx |= 1ULL << 52; else ex = 1;
	if (ey) my |= 1ULL << 52; else ey = 1;
	if (ez) mz |= 1ULL << 52; else ez = 1;
	return (double)fma_core((int)(ux >> 63), mx, ex - 1075, (int)(uy >> 63), my, ey - 1075,
	                        (int)(uz >> 63), mz, ez - 1075, F64);
}

float fmaf(float x, float y, float z)
{
#ifdef __x86_64__
	if (likely(__cpu_features & 8 /* CPU_FMA */)) {
		float r = z;
		__asm__ ("vfmadd231ss %2, %1, %0" : "+x"(r) : "x"(x), "x"(y));
		if (likely(__builtin_fabsf(r) >= FLT_MIN && __builtin_fabsf(r) <= FLT_MAX)) return r;
	}
#endif
	if (!__builtin_isfinite(x) || !__builtin_isfinite(y) || !__builtin_isfinite(z) || x == 0 || y == 0)
		return x * y + z;
	uint32_t ux = asuint(x), uy = asuint(y), uz = asuint(z);
	int ex = (int)(ux >> 23 & 0xff), ey = (int)(uy >> 23 & 0xff), ez = (int)(uz >> 23 & 0xff);
	uint64_t mx = ux & 0x7fffff, my = uy & 0x7fffff, mz = uz & 0x7fffff;
	if (ex) mx |= 1u << 23; else ex = 1;
	if (ey) my |= 1u << 23; else ey = 1;
	if (ez) mz |= 1u << 23; else ez = 1;
	return (float)fma_core((int)(ux >> 31), mx, ex - 150, (int)(uy >> 31), my, ey - 150,
	                       (int)(uz >> 31), mz, ez - 150, F32);
}

long double fmal(long double x, long double y, long double z)
{
	if (!__builtin_isfinite(x) || !__builtin_isfinite(y) || !__builtin_isfinite(z) || x == 0 || y == 0)
		return x * y + z;
	union ldshape a = { x }, b = { y }, c = { z };
	int ex = a.i.se & 0x7fff, ey = b.i.se & 0x7fff, ez = c.i.se & 0x7fff;
	if (!ex) ex = 1;                         /* pseudo-denormal/denormal: integer bit 0 */
	if (!ey) ey = 1;
	if (!ez) ez = 1;
	return fma_core(a.i.se >> 15, a.i.m, ex - 16446, b.i.se >> 15, b.i.m, ey - 16446,
	                c.i.se >> 15, c.i.m, ez - 16446, F80);
}
