/*
 * lib-spfxd — argument reduction modulo pi/2.
 *
 * Moderate arguments (|x| < 2^20 pi/2) use Cody-Waite reduction with pi/2
 * split into three 33-bit pieces plus a tail: each n*piece is exact and
 * the running remainder is tracked as a double-double, giving ~2^-70
 * relative accuracy even for the x closest to a multiple of pi/2.
 *
 * Larger arguments use Payne-Hanek: with x = m 2^e (m an integer of up to
 * 64 bits) only a 256-bit window of the bits of 2/pi matters — earlier
 * bits contribute multiples of 4 to x*2/pi and later ones less than
 * 2^-190.  The window product gives n mod 4 and a 128-bit fraction.  The
 * same routine serves double and long double (src/math, arch math).
 */
#include <math.h>
#include "fp.h"

typedef unsigned __int128 u128;

/* 64 bits of 2/pi starting at bit j (1-indexed after the binary point) */
static uint64_t pi_bits(int j)
{
	int idx = (j - 1) >> 6, off = (j - 1) & 63;
	uint64_t w = __two_over_pi_bits[idx] << off;
	if (off) w |= __two_over_pi_bits[idx + 1] >> (64 - off);
	return w;
}

/* Bits [pos, pos+63] of the little-endian multiword integer p. */
static uint64_t bits_at(const uint64_t *p, int n, int pos)
{
	int idx = pos >> 6, off = pos & 63;
	uint64_t lo = idx < n ? p[idx] : 0;
	uint64_t hi = idx + 1 < n ? p[idx + 1] : 0;
	return off ? (lo >> off) | (hi << (64 - off)) : lo;
}

/* x = m 2^e with m < 2^64 and e >= -40: returns n = round(x 2/pi) mod 4
 * and the signed fraction x 2/pi - n in [-1/2, 1/2] as a 128-bit
 * magnitude f (value f / 2^128) with its sign in *fneg. */
hidden int __rem_pio2_bits(uint64_t m, int e, uint64_t f[2], int *fneg)
{
	int j0 = e - 1 > 1 ? e - 1 : 1;
	uint64_t w[4];
	for (int i = 0; i < 4; i++) w[i] = pi_bits(j0 + 64 * i);
	/* P = m * W, W = w0 w1 w2 w3 (w0 most significant) */
	uint64_t p[5] = { 0 };
	u128 carry = 0;
	for (int i = 3; i >= 0; i--) {
		u128 t = (u128)m * w[i] + carry;
		p[3 - i] = (uint64_t)t;
		carry = t >> 64;
	}
	p[4] = (uint64_t)carry;
	/* value = P 2^(e - j0 - 255); integer part starts at bit sh */
	int sh = j0 + 255 - e;
	int n = (int)(bits_at(p, 5, sh) & 3);
	uint64_t fh = bits_at(p, 5, sh - 64), fl = bits_at(p, 5, sh - 128);
	*fneg = 0;
	if (fh >> 63) {
		/* fraction >= 1/2: round n up and negate the fraction */
		n = (n + 1) & 3;
		fl = ~fl + 1;
		fh = ~fh + (fl == 0);
		*fneg = 1;
	}
	f[0] = fh;
	f[1] = fl;
	return n;
}

static int rem_pio2_large(double x, dd_t *r)
{
	uint64_t u = asuint64(x);
	int e = (int)(u >> 52 & 0x7ff) - 1075;
	uint64_t m = (u & 0x000fffffffffffffULL) | (1ULL << 52);
	uint64_t f[2];
	int fneg;
	int n = __rem_pio2_bits(m, e, f, &fneg);
	/* fraction to dd: normalize so the top bit is set */
	uint64_t fh = f[0], fl = f[1];
	int lz = 0;
	if (!fh) { fh = fl; fl = 0; lz = 64; }
	int s = __builtin_clzll(fh);
	if (s) { fh = (fh << s) | (fl >> (64 - s)); fl <<= s; }
	lz += s;
	double d1 = (double)(fh >> 11);
	double d2 = (double)(((fh & 0x7ff) << 42) | (fl >> 22));
	dd_t fr = fast_two_sum(d1 * pow2i(-53 - lz), d2 * pow2i(-106 - lz));
	dd_t res = dd_mul(fr, dd_c(__pio2_dd));
	if (fneg) res = dd_neg(res);
	if (u >> 63) {
		res = dd_neg(res);
		n = -n;
	}
	*r = res;
	return n;
}

int __rem_pio2(double x, dd_t *r)
{
	double ax = fabs(x);
	/* (|x| <= pi/4 needs no special case: n = 0 and r = x exactly) */
	if (likely(ax < 0x1.921fb54442d18p20)) {
		double z = x * __two_over_pi;
		int n = iround(z);
		double nd = (double)n;
		double y1 = x - nd * __pio2_split[0];
		dd_t a = two_sum(y1, -nd * __pio2_split[1]);
		dd_t s = two_sum(a.hi, -nd * __pio2_split[2]);
		s.lo += a.lo - nd * __pio2_split[3];
		*r = fast_two_sum(s.hi, s.lo);
		return n;
	}
	return rem_pio2_large(x, r);
}
