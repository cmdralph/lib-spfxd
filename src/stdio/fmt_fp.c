/*
 * lib-spfxd — floating-point formatting for printf (%e %f %g %a and caps).
 *
 * Decimal conversions are exact.  A finite nonzero value is m * 2^e with an
 * integer m.  For e >= 0 the value is the integer N = m * 2^e; for e < 0 it
 * is N / 10^s with N = m * 5^s and s = -e, because m / 2^s = m*5^s / 10^s.
 * N is built in base 10^9 limbs (multiplying by 2^29 or 5^13 per step), so
 * every decimal digit of the value is known exactly.  Rounding to the
 * requested precision then inspects the exact digits after the cut, which
 * makes ties round to even (or follow the current rounding mode) with no
 * error at any precision.  Digits are streamed straight from the limbs,
 * so even %.5000f or %Lf of LDBL_MAX needs no buffer beyond the limbs.
 *
 * Limb storage: the largest N arises for the smallest long double
 * subnormal (about 11,500 digits), which fits in 1300 limbs.
 */
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "stdio_impl.h"

#define LIMB 1000000000u
#define MAX_LIMBS 1300

struct dec {
	uint32_t l[MAX_LIMBS];   /* little-endian base 1e9 */
	int n;                   /* limbs in use */
	int D;                   /* decimal digits in N */
	int frac;                /* digits after the decimal point */
};

static const uint32_t pow10_tab[10] = {
	1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000
};

static void mul_small(struct dec *x, uint32_t f)
{
	uint64_t carry = 0;
	for (int i = 0; i < x->n; i++) {
		uint64_t t = (uint64_t)x->l[i] * f + carry;
		x->l[i] = (uint32_t)(t % LIMB);
		carry = t / LIMB;
	}
	while (carry) {
		x->l[x->n++] = (uint32_t)(carry % LIMB);
		carry /= LIMB;
	}
}

static void build(struct dec *x, uint64_t m, int e)
{
	int tz = __builtin_ctzll(m);
	m >>= tz;
	e += tz;
	x->n = 0;
	while (m) {
		x->l[x->n++] = (uint32_t)(m % LIMB);
		m /= LIMB;
	}
	if (e >= 0) {
		x->frac = 0;
		while (e > 0) {
			int k = e < 29 ? e : 29;
			mul_small(x, 1u << k);
			e -= k;
		}
	} else {
		int s = -e;
		x->frac = s;
		for (; s >= 13; s -= 13) mul_small(x, 1220703125u);   /* 5^13 */
		if (s) {
			uint32_t p = 1;
			while (s--) p *= 5;
			mul_small(x, p);
		}
	}
	uint32_t top = x->l[x->n - 1];
	int td = 1;
	while (td < 9 && top >= pow10_tab[td]) td++;
	x->D = (x->n - 1) * 9 + td;
}

/* Digit k of N counted from the most significant (0); zero outside N. */
static int digit(const struct dec *x, int k)
{
	if (k < 0 || k >= x->D) return 0;
	int fromlow = x->D - 1 - k;                /* position from the right */
	uint32_t limb = x->l[fromlow / 9];
	return (int)(limb / pow10_tab[fromlow % 9] % 10);
}

/* Any nonzero digit at positions > k? */
static int sticky_after(const struct dec *x, int k)
{
	if (k < -1) return 1;                      /* all of N lies after k */
	if (k >= x->D - 1) return 0;
	int fromlow = x->D - 2 - k;                /* position of digit k+1 from the right */
	int li = fromlow / 9, pos = fromlow % 9;
	if (x->l[li] % pow10_tab[pos + 1]) return 1;
	for (int i = 0; i < li; i++)
		if (x->l[i]) return 1;
	return 0;
}

static int rounding_mode(void)
{
	unsigned csr;
	__asm__ __volatile__ ("stmxcsr %0" : "=m"(csr));
	return (int)((csr >> 13) & 3);             /* 0 near, 1 down, 2 up, 3 zero */
}

/*
 * Decide whether keeping digits [.., K) rounds up.  On a round-up, *carry
 * receives the highest position that changes: that digit is incremented
 * and everything after it becomes zero.
 */
static int round_up(const struct dec *x, int K, int neg, int *carry)
{
	int d = digit(x, K), st = sticky_after(x, K), up;
	switch (rounding_mode()) {
	case 0:
		up = d > 5 || (d == 5 && (st || (digit(x, K - 1) & 1)));
		break;
	case 1: up = neg && (d || st); break;
	case 2: up = !neg && (d || st); break;
	default: up = 0; break;
	}
	if (up) {
		int c = K - 1;
		while (c >= 0 && c < x->D && digit(x, c) == 9) c--;
		*carry = c;
	}
	return up;
}

static int rdigit(const struct dec *x, int k, int up, int carry)
{
	if (!up || k < carry) return digit(x, k);
	return k == carry ? digit(x, k) + 1 : 0;
}

/* ---------------------------------------------------------------------- */

static void out(FILE *f, const char *s, size_t l)
{
	if (l && !(f->flags & F_ERR)) __fwritex((const unsigned char *)s, l, f);
}

static void repeat(FILE *f, char c, long n)
{
	char buf[64];
	if (n <= 0) return;
	memset(buf, c, n < 64 ? (size_t)n : 64);
	for (; n > 64; n -= 64) out(f, buf, 64);
	out(f, buf, (size_t)n);
}

/* Stream digits [from, to) of the rounded value. */
static void out_digits(FILE *f, const struct dec *x, int from, int to, int up, int carry)
{
	char buf[64];
	int n = 0;
	for (int k = from; k < to; k++) {
		buf[n++] = (char)('0' + rdigit(x, k, up, carry));
		if (n == sizeof buf) {
			out(f, buf, (size_t)n);
			n = 0;
		}
	}
	out(f, buf, (size_t)n);
}

static long pad_and_sign(FILE *f, int w, unsigned fl, char sign, long len, int pre)
{
	long total = len + (sign != 0);
	if (total > INT_MAX) return -1;
	long width = w > total ? w : total;
	if (pre) {
		if (!(fl & (FL_LEFT | FL_ZERO))) repeat(f, ' ', width - total);
		if (sign) out(f, &sign, 1);
		if (!(fl & FL_LEFT) && (fl & FL_ZERO)) repeat(f, '0', width - total);
	} else if (fl & FL_LEFT) {
		repeat(f, ' ', width - total);
	}
	return width;
}

/* %e: one digit, point, p digits, exponent */
static long fmt_e(FILE *f, const struct dec *x, int neg, char sign, int w, int p, unsigned fl, int upper)
{
	int carry = 0, up = round_up(x, p + 1, neg, &carry);
	int X = x->D - 1 - x->frac;
	int lead_one = up && carry < 0;
	if (lead_one) X++;
	char eb[8];
	int el = 0, ax = X < 0 ? -X : X;
	char tmp[8];
	int tl = 0;
	do { tmp[tl++] = (char)('0' + ax % 10); ax /= 10; } while (ax);
	if (tl < 2) tmp[tl++] = '0';
	eb[el++] = upper ? 'E' : 'e';
	eb[el++] = X < 0 ? '-' : '+';
	while (tl) eb[el++] = tmp[--tl];

	int point = p > 0 || (fl & FL_ALT);
	long len = 1 + point + (long)p + el;
	long width = pad_and_sign(f, w, fl, sign, len, 1);
	if (width < 0) return -1;
	if (lead_one) {
		out(f, "1", 1);
		if (point) out(f, ".", 1);
		repeat(f, '0', p);
	} else {
		out_digits(f, x, 0, 1, up, carry);
		if (point) out(f, ".", 1);
		out_digits(f, x, 1, p + 1, up, carry);
	}
	out(f, eb, (size_t)el);
	pad_and_sign(f, w, fl, sign, len, 0);
	return width;
}

/* %f: integer part, point, p fraction digits */
static long fmt_f(FILE *f, const struct dec *x, int neg, char sign, int w, int p, unsigned fl)
{
	int I = x->D - x->frac;                    /* digits before the point */
	int carry = 0, up = round_up(x, I + p, neg, &carry);
	int first = I > 0 ? 0 : I - 1;             /* position of the leading integer digit */
	int lead_one = up && carry < first;
	long ilen = (long)(I > 0 ? I : 1) + lead_one;
	int point = p > 0 || (fl & FL_ALT);
	long len = ilen + point + (long)p;
	long width = pad_and_sign(f, w, fl, sign, len, 1);
	if (width < 0) return -1;
	if (lead_one) out(f, "1", 1);
	out_digits(f, x, first, I, up, carry);
	if (point) out(f, ".", 1);
	out_digits(f, x, I, I + p, up, carry);
	pad_and_sign(f, w, fl, sign, len, 0);
	return width;
}

/* %g: choose %e or %f from the exponent after rounding to P digits */
static long fmt_g(FILE *f, const struct dec *x, int neg, char sign, int w, int p, unsigned fl, int upper)
{
	int P = p < 0 ? 6 : p == 0 ? 1 : p;
	int carry = 0, up = round_up(x, P, neg, &carry);
	int X = x->D - 1 - x->frac;
	if (up && carry < 0) X++;

	/* significant digits after stripping trailing zeros */
	int S = P;
	if (!(fl & FL_ALT)) {
		if (up && carry < 0) S = 1;
		else {
			while (S > 1 && rdigit(x, S - 1, up, carry) == 0) S--;
		}
	}
	if (X < P && X >= -4) {
		int fp = (fl & FL_ALT) ? P - 1 - X : S - 1 - X;
		if (fp < 0) fp = 0;
		return fmt_f(f, x, neg, sign, w, fp, fl);
	}
	return fmt_e(f, x, neg, sign, w, (fl & FL_ALT) ? P - 1 : S - 1, fl, upper);
}

/* %a: hexadecimal significand.  Doubles print as 0x1.hhhh (glibc style);
 * long doubles show the x87 explicit integer bit: 0x8.hhhh, 0xf.hhhh. */
static long fmt_a(FILE *f, uint64_t m, int e, int is_zero, int neg, char sign,
	int w, int p, unsigned fl, int upper)
{
	const char *hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	int lead;
	uint64_t frac;
	int fbits;

	if (is_zero) {
		lead = 0;
		frac = 0;
		fbits = 52;
		e = 0;
	} else if (fl & FL_LDBL) {
		lead = (int)(m >> 60);
		frac = m << 4;
		fbits = 60;
		e = e + 63 - 3;
	} else {
		/* m holds the 64-bit x87 significand of an exactly converted
		 * double; renormalize to 1.fraction (or 0.fraction for doubles
		 * that were subnormal). */
		int E = e + 63;                     /* value = m/2^63 * 2^E */
		if (E < -1022) {
			int sh = -1022 - E;
			frac = sh < 64 ? m >> sh : 0;
			E = -1022;
		} else {
			frac = m;
		}
		lead = (int)(frac >> 63);
		frac <<= 1;
		fbits = 52;
		frac >>= 12;                        /* 52 fraction bits, right aligned */
		frac <<= 12;
		e = E;
	}
	/* frac: fraction bits left-aligned in 64 bits; fbits meaningful */
	int ndig = (fbits + 3) / 4;
	if (p >= 0 && p < ndig) {
		int drop = 64 - 4 * p;                  /* bits below the kept digits */
		uint64_t kept = p ? frac >> drop : 0;
		uint64_t rem = drop >= 64 ? frac : frac << (64 - drop) >> (64 - drop);
		uint64_t half = 1ULL << (drop - 1);
		int up;
		switch (rounding_mode()) {
		case 0: up = rem > half || (rem == half && ((p ? kept : (uint64_t)lead) & 1)); break;
		case 1: up = neg && rem; break;
		case 2: up = !neg && rem; break;
		default: up = 0; break;
		}
		if (up) {
			kept++;
			if (p == 0 || kept >> (4 * p)) {
				kept = 0;
				lead++;
				/* a carry into the leading digit of a double is kept
				 * as 0x2p+e (as glibc prints it) */
				if (lead == 16) {
					lead = 1;
					e += 4;
				}
			}
		}
		frac = p ? kept << drop : 0;
		ndig = p;
	} else if (p < 0) {
		while (ndig > 0 && !((frac >> (64 - 4 * ndig)) & 15)) ndig--;
	}

	char body[48];
	int bl = 0;
	body[bl++] = '0';
	body[bl++] = upper ? 'X' : 'x';
	body[bl++] = hex[lead];
	int fraction_digits = p > ndig ? p : ndig;
	if (fraction_digits > 0 || (fl & FL_ALT)) body[bl++] = '.';
	for (int i = 0; i < ndig; i++) body[bl++] = hex[(frac >> (60 - 4 * i)) & 15];
	long extra_zeros = fraction_digits - ndig;

	char eb[8];
	int el = 0, ax = e < 0 ? -e : e;
	char tmp[8];
	int tl = 0;
	do { tmp[tl++] = (char)('0' + ax % 10); ax /= 10; } while (ax);
	eb[el++] = upper ? 'P' : 'p';
	eb[el++] = e < 0 ? '-' : '+';
	while (tl) eb[el++] = tmp[--tl];

	long len = bl + extra_zeros + el;
	long total = len + (sign != 0);
	if (total > INT_MAX) return -1;
	long width = w > total ? w : total;
	if (!(fl & (FL_LEFT | FL_ZERO))) repeat(f, ' ', width - total);
	if (sign) out(f, &sign, 1);
	out(f, body, 2);
	if (!(fl & FL_LEFT) && (fl & FL_ZERO)) repeat(f, '0', width - total);
	out(f, body + 2, (size_t)(bl - 2));
	repeat(f, '0', extra_zeros);
	out(f, eb, (size_t)el);
	if (fl & FL_LEFT) repeat(f, ' ', width - total);
	return width;
}

hidden int __fmt_fp(FILE *f, long double y, int w, int p, unsigned fl, int conv)
{
	union { long double f; struct { uint64_t m; uint16_t se; } i; } u = { .f = y };
	int neg = u.i.se >> 15;
	int bexp = u.i.se & 0x7fff;
	uint64_t m = u.i.m;
	char sign = neg ? '-' : (fl & FL_PLUS) ? '+' : (fl & FL_SPACE) ? ' ' : 0;
	int upper = conv >= 'A' && conv <= 'Z';
	int lc = conv | 32;

	if (bexp == 0x7fff) {
		/* infinity (significand 0x8000...) or NaN; never zero padded */
		const char *s = (m << 1) ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
		long total = 3 + (sign != 0);
		long width = w > total ? w : total;
		if (!(fl & FL_LEFT)) repeat(f, ' ', width - total);
		if (sign) out(f, &sign, 1);
		out(f, s, 3);
		if (fl & FL_LEFT) repeat(f, ' ', width - total);
		return (int)width;
	}

	int is_zero = m == 0;
	/* value = m * 2^(e) with e for the integer significand */
	int e = (bexp ? bexp : 1) - 16383 - 63;

	if (lc == 'a') return (int)fmt_a(f, m, e, is_zero, neg, sign, w, p, fl, upper);

	if (p < 0) p = 6;
	struct dec x;
	if (is_zero) {
		x.n = 1;
		x.l[0] = 0;
		x.D = 1;
		x.frac = 0;
	} else {
		build(&x, m, e);
	}
	long r;
	switch (lc) {
	case 'e': r = fmt_e(f, &x, neg, sign, w, p, fl, upper); break;
	case 'f': r = fmt_f(f, &x, neg, sign, w, p, fl); break;
	default:  r = fmt_g(f, &x, neg, sign, w, p, fl, upper); break;
	}
	return (int)r;
}
