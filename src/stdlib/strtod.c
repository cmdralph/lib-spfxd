/*
 * lib-spfxd — strtof / strtod / strtold: correctly rounded conversion of
 * decimal and hexadecimal strings, honoring the current rounding mode.
 *
 * Decimal input is reduced to an integer significand D (with up to MAXDIG
 * significant digits; any further nonzero digits only set a sticky bit) and
 * a power of ten E, so the value is D * 10^E = D * 5^E * 2^E.
 *
 *   Fast path (Clinger): when D and 10^|E| are exactly representable in the
 *   target type, one multiplication or division by the hardware gives the
 *   correctly rounded result (in any rounding mode).
 *
 *   Exact path: big-integer arithmetic.  For E >= 0 the value is the
 *   integer D*5^E scaled by 2^E.  For E < 0 it is D / 5^-E scaled by 2^E;
 *   the quotient is computed with at least P+2 significant bits and the
 *   remainder folded into a sticky bit.  Rounding then inspects exact bits,
 *   so ties and directed rounding are always right.
 *
 * MAXDIG bounds the digits that can influence rounding: the exact midpoint
 * between two adjacent values of the target type has at most ~770 (double)
 * or ~11,500 (long double) significant digits, so keeping that many and
 * folding the rest into the sticky bit is exact.
 */
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include "stdio_impl.h"

/* ---------------------------------------------------------------------- */
/* minimal big integers (base 2^32, little-endian)                         */
/* ---------------------------------------------------------------------- */

struct big {
	uint32_t *d;
	int n;
	int cap;
};

static void big_norm(struct big *a)
{
	while (a->n && !a->d[a->n - 1]) a->n--;
}

static void big_mul_small(struct big *a, uint32_t m)
{
	uint64_t carry = 0;
	for (int i = 0; i < a->n; i++) {
		uint64_t t = (uint64_t)a->d[i] * m + carry;
		a->d[i] = (uint32_t)t;
		carry = t >> 32;
	}
	if (carry) a->d[a->n++] = (uint32_t)carry;
}

static void big_add_small(struct big *a, uint32_t v)
{
	for (int i = 0; v && i < a->n; i++) {
		uint64_t t = (uint64_t)a->d[i] + v;
		a->d[i] = (uint32_t)t;
		v = (uint32_t)(t >> 32);
	}
	if (v) a->d[a->n++] = v;
}

static void big_mul_pow5(struct big *a, int e)
{
	for (; e >= 13; e -= 13) big_mul_small(a, 1220703125u);
	static const uint32_t p5[13] = { 1, 5, 25, 125, 625, 3125, 15625, 78125, 390625,
		1953125, 9765625, 48828125, 244140625 };
	if (e) big_mul_small(a, p5[e]);
}

static int big_bitlen(const struct big *a)
{
	if (!a->n) return 0;
	return (a->n - 1) * 32 + 32 - __builtin_clz(a->d[a->n - 1]);
}

static void big_shl(struct big *a, int s)
{
	int w = s / 32, b = s % 32;
	if (!a->n) return;
	if (b) {
		uint32_t carry = 0;
		for (int i = 0; i < a->n; i++) {
			uint32_t v = a->d[i];
			a->d[i] = (v << b) | carry;
			carry = v >> (32 - b);
		}
		if (carry) a->d[a->n++] = carry;
	}
	if (w) {
		memmove(a->d + w, a->d, (size_t)a->n * 4);
		memset(a->d, 0, (size_t)w * 4);
		a->n += w;
	}
}

static int big_bit(const struct big *a, long i)
{
	if (i < 0 || i / 32 >= a->n) return 0;
	return (a->d[i / 32] >> (i % 32)) & 1;
}

/* Any bit set below position i? */
static int big_any_below(const struct big *a, long i)
{
	if (i <= 0) return 0;
	long w = i / 32;
	if (w >= a->n) w = a->n, i = (long)a->n * 32;
	for (long k = 0; k < w; k++)
		if (a->d[k]) return 1;
	if (i % 32 && w < a->n && (a->d[w] & ((1u << (i % 32)) - 1))) return 1;
	return 0;
}

/* Bits [lo, lo+64) as an integer (lo may be negative). */
static uint64_t big_extract(const struct big *a, long lo, int cnt)
{
	uint64_t r = 0;
	for (int i = cnt - 1; i >= 0; i--) r = r << 1 | (uint64_t)big_bit(a, lo + i);
	return r;
}

/*
 * q = u / v, u = remainder (Knuth, TAOCP vol. 2, 4.3.1, algorithm D).
 * u needs one spare limb.  v must be normalized-able (nonzero).
 */
static void big_divmod(struct big *u, const struct big *v0, struct big *q, struct big *vtmp)
{
	int n = v0->n, m = u->n - v0->n;
	if (m < 0) {
		q->n = 0;
		return;
	}
	if (n == 1) {
		uint64_t r = 0;
		uint32_t dv = v0->d[0];
		for (int i = u->n - 1; i >= 0; i--) {
			uint64_t cur = (r << 32) | u->d[i];
			q->d[i] = (uint32_t)(cur / dv);
			r = cur % dv;
		}
		q->n = u->n;
		big_norm(q);
		u->d[0] = (uint32_t)r;
		u->n = 1;
		big_norm(u);
		return;
	}
	/* normalize so the divisor's top bit is set */
	int s = __builtin_clz(v0->d[n - 1]);
	memcpy(vtmp->d, v0->d, (size_t)n * 4);
	vtmp->n = n;
	uint32_t *v = vtmp->d;
	if (s) {
		for (int i = n - 1; i > 0; i--) v[i] = (v[i] << s) | (v[i - 1] >> (32 - s));
		v[0] <<= s;
		uint32_t carry = 0;
		for (int i = 0; i < u->n; i++) {
			uint32_t x = u->d[i];
			u->d[i] = (x << s) | carry;
			carry = x >> (32 - s);
		}
		u->d[u->n] = carry;
	} else {
		u->d[u->n] = 0;
	}
	uint32_t *un = u->d;
	for (int j = m; j >= 0; j--) {
		uint64_t num = ((uint64_t)un[j + n] << 32) | un[j + n - 1];
		uint64_t qhat = num / v[n - 1], rhat = num % v[n - 1];
		while (qhat >> 32 || qhat * v[n - 2] > ((rhat << 32) | un[j + n - 2])) {
			qhat--;
			rhat += v[n - 1];
			if (rhat >> 32) break;
		}
		int64_t borrow = 0;
		uint64_t carry = 0;
		for (int i = 0; i < n; i++) {
			uint64_t p = qhat * v[i] + carry;
			carry = p >> 32;
			int64_t t = (int64_t)un[i + j] - (int64_t)(uint32_t)p + borrow;
			un[i + j] = (uint32_t)t;
			borrow = t >> 32;
		}
		int64_t t = (int64_t)un[j + n] - (int64_t)carry + borrow;
		un[j + n] = (uint32_t)t;
		if (t < 0) {
			/* qhat was one too large: add the divisor back */
			qhat--;
			uint64_t c = 0;
			for (int i = 0; i < n; i++) {
				uint64_t sum = (uint64_t)un[i + j] + v[i] + c;
				un[i + j] = (uint32_t)sum;
				c = sum >> 32;
			}
			un[j + n] += (uint32_t)c;
		}
		q->d[j] = (uint32_t)qhat;
	}
	q->n = m + 1;
	big_norm(q);
	/* remainder (still scaled by 2^s; only its zeroness is needed) */
	u->n = n;
	big_norm(u);
}

/* ---------------------------------------------------------------------- */
/* target formats                                                          */
/* ---------------------------------------------------------------------- */

struct fmt {
	int P;          /* significand bits */
	int emin, emax; /* exponent range of the leading bit for normals */
};

static const struct fmt formats[3] = {
	{ 24, -126, 127 },
	{ 53, -1022, 1023 },
	{ 64, -16382, 16383 },
};

static int rounding_mode(void)
{
	unsigned csr;
	__asm__ __volatile__ ("stmxcsr %0" : "=m"(csr));
	return (int)((csr >> 13) & 3);       /* 0 nearest, 1 down, 2 up, 3 zero */
}

/* Result: value = m * 2^e (m < 2^P), or an overflow/zero indication. */
struct conv {
	uint64_t m;
	int e;
	int inexact;
	int tiny;       /* below the normal range before rounding */
	int overflow;
};

static int should_round_up(int mode, int neg, int lsb, int half, int below)
{
	switch (mode) {
	case 0: return half && (below || lsb);
	case 1: return neg && (half || below);
	case 2: return !neg && (half || below);
	default: return 0;
	}
}

/* Round the exact value N * 2^s (+ sticky) to the format. */
static void round_big(const struct big *N, long s, int sticky, const struct fmt *f,
	int neg, int mode, struct conv *r)
{
	long L = big_bitlen(N);
	long X = L - 1 + s;          /* exponent of the leading bit */
	long keep = f->P;
	memset(r, 0, sizeof *r);
	if (X < f->emin) {
		keep = f->P - (f->emin - X);
		r->tiny = 1;
	}
	long cut = L - keep;         /* bits discarded */
	uint64_t m;
	if (cut <= 0) {
		m = big_extract(N, 0, (int)L) << (-cut);
		r->m = m;
		r->e = (int)(s + cut);
		r->inexact = sticky;
		if (sticky && should_round_up(mode, neg, (int)(m & 1), 0, 1)) {
			r->m++;
			if (!r->m) {                 /* 64-bit significand wrapped */
				r->m = 1ULL << 63;
				r->e++;
			}
		}
	} else {
		m = keep > 0 ? big_extract(N, cut, (int)keep) : 0;
		int half = big_bit(N, cut - 1);
		int below = big_any_below(N, cut - 1) || sticky;
		r->inexact = half || below;
		r->e = (int)(s + cut);
		if (should_round_up(mode, neg, (int)(m & 1), half, below)) {
			m++;
			if (!m) {                     /* 64-bit significand wrapped */
				m = 1ULL << 63;
				r->e++;
			}
		}
		r->m = m;
	}
	/* carry out of the significand (P < 64; a 64-bit carry is handled
	 * above, and shifting a uint64_t by 64 would be undefined) */
	if (f->P < 64 && (r->m >> f->P)) {
		r->m >>= 1;
		r->e++;
	}
	/* leading-bit exponent of the result */
	if (r->m) {
		int lead = 63 - __builtin_clzll(r->m) + r->e;
		if (lead > f->emax) r->overflow = 1;
	}
}

/* ---------------------------------------------------------------------- */
/* assembling IEEE values                                                  */
/* ---------------------------------------------------------------------- */

static long double make_value(const struct conv *c, int prec, int neg, int mode)
{
	const struct fmt *f = &formats[prec];
	if (c->overflow) {
		/* round-to-nearest and away-from-zero modes give infinity */
		int to_inf = mode == 0 || (mode == 1 && neg) || (mode == 2 && !neg);
		errno = ERANGE;
		if (prec == 0) {
			float v = to_inf ? HUGE_VALF : FLT_MAX;
			return neg ? -v : v;
		}
		if (prec == 1) {
			double v = to_inf ? HUGE_VAL : DBL_MAX;
			return neg ? -v : v;
		}
		long double v = to_inf ? HUGE_VALL : LDBL_MAX;
		return neg ? -v : v;
	}
	if (c->tiny && c->inexact) errno = ERANGE;
	if (!c->m) return neg ? -0.0L : 0.0L;

	/* normalize m to P bits; subnormals keep the minimum exponent */
	uint64_t m = c->m;
	int e = c->e;
	int lead = 63 - __builtin_clzll(m);
	int shift = (f->P - 1) - lead;
	int target_e = e - shift;                 /* value = (m<<shift) * 2^target_e */
	int min_e = f->emin - (f->P - 1);
	if (target_e < min_e) {
		shift -= min_e - target_e;
		target_e = min_e;
	}
	m = shift >= 0 ? m << shift : m >> -shift;
	int biased = (m >> (f->P - 1)) ? target_e + (f->P - 1) - f->emin + 1 : 0;

	if (prec == 0) {
		union { float f; uint32_t i; } u;
		u.i = (uint32_t)(m & 0x7fffff) | (uint32_t)biased << 23 | (uint32_t)neg << 31;
		return u.f;
	}
	if (prec == 1) {
		union { double f; uint64_t i; } u;
		u.i = (m & 0xfffffffffffffULL) | (uint64_t)biased << 52 | (uint64_t)neg << 63;
		return u.f;
	}
	union { long double f; struct { uint64_t m; uint16_t se; } i; } u;
	u.i.m = m;
	u.i.se = (uint16_t)(biased | neg << 15);
	return u.f;
}

/* ---------------------------------------------------------------------- */
/* decimal conversion                                                      */
/* ---------------------------------------------------------------------- */

static const double pow10_dbl[23] = {
	1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
	1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22
};

/*
 * digits: first significant digit; ndig significant digits are taken from
 * the string (skipping one '.'), value = D * 10^e10, sticky = discarded
 * nonzero digits.
 */
static long double decimal_to_fp(const char *digits, int ndig, long e10, int sticky,
	int prec, int neg)
{
	const struct fmt *f = &formats[prec];
	int mode = rounding_mode();

	/* Clinger's fast path */
	if (!sticky && ndig <= 19) {
		uint64_t D = 0;
		const char *p = digits;
		for (int i = 0; i < ndig; p++) {
			if (*p == '.') continue;
			D = D * 10 + (uint64_t)(*p - '0');
			i++;
		}
		if (prec == 1 && D < (1ULL << 53) && e10 >= -22 && e10 <= 22) {
			double v = (double)D;
			v = e10 < 0 ? v / pow10_dbl[-e10] : v * pow10_dbl[e10];
			return neg ? -v : v;
		}
		if (prec == 0 && D < (1ULL << 24) && e10 >= -10 && e10 <= 10) {
			float v = (float)D;
			v = e10 < 0 ? v / (float)pow10_dbl[-e10] : v * (float)pow10_dbl[e10];
			return neg ? -v : v;
		}
		if (prec == 2 && e10 >= -27 && e10 <= 27) {
			long double v = (long double)D, p10 = 1;
			for (long i = 0, k = e10 < 0 ? -e10 : e10; i < k; i++) p10 *= 10;
			v = e10 < 0 ? v / p10 : v * p10;
			return neg ? -v : v;
		}
	}

	/* Out-of-range shortcuts keep the big integers bounded. */
	long top = e10 + ndig;                  /* value < 10^top */
	long max10 = prec == 0 ? 39 : prec == 1 ? 310 : 4933;
	long min10 = prec == 0 ? -46 : prec == 1 ? -324 : -4952;
	struct conv c;
	memset(&c, 0, sizeof c);
	if (top > max10) {
		c.overflow = 1;
		return make_value(&c, prec, neg, mode);
	}
	if (top < min10) {
		/* far below the smallest subnormal: 0, or the smallest subnormal
		 * when rounding away from zero */
		c.tiny = c.inexact = 1;
		if ((mode == 1 && neg) || (mode == 2 && !neg)) {
			c.m = 1;
			c.e = f->emin - (f->P - 1);
		}
		return make_value(&c, prec, neg, mode);
	}

	/* Sizes in 32-bit limbs: D, 5^|e10|, the shifted numerator, quotient. */
	long d_limbs = ndig / 9 + 2;
	long p_limbs = (labs(e10) * 2322 / 1000) / 32 + 3;
	long each = d_limbs + p_limbs + f->P / 32 + 8;
	long total = 4 * each;
	uint32_t stackbuf[1024], *mem = stackbuf;
	if (total > 1024) {
		mem = malloc((size_t)total * 4);
		if (!mem) {
			errno = ENOMEM;
			return 0;
		}
	}
	struct big D = { mem, 0, (int)each };
	struct big Pw = { mem + each, 0, (int)each };
	struct big Q = { mem + 2 * each, 0, (int)each };
	struct big vtmp = { mem + 3 * each, 0, (int)each };

	/* D from the digit string, nine digits at a time */
	const char *p = digits;
	int left = ndig;
	while (left) {
		uint32_t chunk = 0, mul = 1;
		int k = left < 9 ? left : 9;
		for (int i = 0; i < k; p++) {
			if (*p == '.') continue;
			chunk = chunk * 10 + (uint32_t)(*p - '0');
			mul *= 10;
			i++;
		}
		big_mul_small(&D, mul);
		big_add_small(&D, chunk);
		left -= k;
	}

	long s;
	if (e10 >= 0) {
		big_mul_pow5(&D, (int)e10);
		s = e10;
		round_big(&D, s, sticky, f, neg, mode, &c);
	} else {
		Pw.d[0] = 1;
		Pw.n = 1;
		big_mul_pow5(&Pw, (int)-e10);
		long t = f->P + 3 + big_bitlen(&Pw) - big_bitlen(&D);
		if (t < 0) t = 0;
		big_shl(&D, (int)t);
		big_divmod(&D, &Pw, &Q, &vtmp);
		sticky |= D.n != 0;
		s = e10 - t;
		round_big(&Q, s, sticky, f, neg, mode, &c);
	}
	if (mem != stackbuf) free(mem);
	return make_value(&c, prec, neg, mode);
}

/* ---------------------------------------------------------------------- */
/* hexadecimal conversion                                                  */
/* ---------------------------------------------------------------------- */

static long double hex_to_fp(unsigned __int128 m, long e2, int sticky, int prec, int neg)
{
	const struct fmt *f = &formats[prec];
	uint32_t limbs[5] = { (uint32_t)m, (uint32_t)(m >> 32), (uint32_t)(m >> 64), (uint32_t)(m >> 96), 0 };
	struct big N = { limbs, 4, 5 };
	struct conv c;
	big_norm(&N);
	if (!N.n) return neg ? -0.0L : 0.0L;
	/* clamp absurd exponents (the value is far outside every range) */
	if (e2 > 100000) e2 = 100000;
	if (e2 < -100000) e2 = -100000;
	round_big(&N, e2, sticky, f, neg, rounding_mode(), &c);
	return make_value(&c, prec, neg, rounding_mode());
}

/* ---------------------------------------------------------------------- */
/* parsing                                                                 */
/* ---------------------------------------------------------------------- */

static int ci_prefix(const char *s, const char *word)
{
	int i = 0;
	for (; word[i]; i++)
		if ((s[i] | 32) != word[i]) return 0;
	return i;
}

static int hexval(int c)
{
	if (c - '0' < 10u) return c - '0';
	c |= 32;
	if (c - 'a' < 6u) return c - 'a' + 10;
	return -1;
}

/* Parse an exponent "e[+-]digits" / "p[+-]digits"; returns chars used (0 if
 * malformed, in which case the marker is not part of the number). */
static int parse_exp(const char *s, long *out)
{
	const char *p = s + 1;
	int neg = 0;
	if (*p == '+' || *p == '-') neg = *p++ == '-';
	if (*p - '0' >= 10u) return 0;
	long v = 0;
	for (; *p - '0' < 10u; p++)
		if (v < 100000000) v = v * 10 + (*p - '0');
	*out = neg ? -v : v;
	return (int)(p - s);
}

hidden long double __strtold_internal(const char *restrict s0, char **restrict end, int prec)
{
	const char *s = s0;
	int neg = 0, n;
	const int maxdig = prec == 2 ? 11600 : 800;

	while (isspace((unsigned char)*s)) s++;
	if (*s == '+' || *s == '-') neg = *s++ == '-';

	if ((n = ci_prefix(s, "inf"))) {
		s += n;
		if ((n = ci_prefix(s, "inity"))) s += n;
		if (end) *end = (char *)s;
		return neg ? -HUGE_VALL : HUGE_VALL;
	}
	if ((n = ci_prefix(s, "nan"))) {
		s += n;
		if (*s == '(') {
			const char *p = s + 1;
			while (isalnum((unsigned char)*p) || *p == '_') p++;
			if (*p == ')') s = p + 1;
		}
		if (end) *end = (char *)s;
		long double nanv = __builtin_nanl("");
		return neg ? -nanv : nanv;
	}

	if (s[0] == '0' && (s[1] | 32) == 'x' &&
	    (hexval((unsigned char)s[2]) >= 0 || (s[2] == '.' && hexval((unsigned char)s[3]) >= 0))) {
		/* hexadecimal significand */
		const char *p = s + 2;
		unsigned __int128 m = 0;   /* room for 64 significant bits plus guard bits */
		long e2 = 0;
		int sticky = 0, seen_point = 0, any = 0;
		for (;; p++) {
			int v;
			if (*p == '.' && !seen_point) {
				seen_point = 1;
				continue;
			}
			if ((v = hexval((unsigned char)*p)) < 0) break;
			any = 1;
			if (m >> 120) {
				/* significand full: keep the digit only as sticky */
				sticky |= v != 0;
				if (!seen_point) e2 += 4;
			} else {
				m = m << 4 | (unsigned)v;
				if (seen_point) e2 -= 4;
			}
		}
		(void)any;
		long pe;
		if ((*p | 32) == 'p' && (n = parse_exp(p, &pe))) {
			e2 += pe;
			p += n;
		}
		if (end) *end = (char *)p;
		return hex_to_fp(m, e2, sticky, prec, neg);
	}

	/* decimal: [digits][.digits][e[+-]digits] */
	const char *p = s, *first = 0;
	int ndig = 0, sticky = 0, seen_point = 0, any = 0;
	long e10 = 0;
	for (;; p++) {
		if (*p == '.' && !seen_point) {
			seen_point = 1;
			continue;
		}
		if (*p - '0' >= 10u) break;
		any = 1;
		if (!first && *p == '0') {
			if (seen_point) e10--;
			continue;
		}
		if (!first) first = p;
		if (ndig < maxdig) {
			ndig++;
			if (seen_point) e10--;
		} else {
			sticky |= *p != '0';
			if (!seen_point) e10++;
		}
	}
	if (!any) {
		if (end) *end = (char *)s0;
		return 0;
	}
	long pe;
	if ((*p | 32) == 'e' && (n = parse_exp(p, &pe))) {
		e10 += pe;
		p += n;
	}
	if (end) *end = (char *)p;
	if (!first) return neg ? -0.0L : 0.0L;
	return decimal_to_fp(first, ndig, e10, sticky, prec, neg);
}

float strtof(const char *restrict s, char **restrict end)
{
	return (float)__strtold_internal(s, end, 0);
}

double strtod(const char *restrict s, char **restrict end)
{
	return (double)__strtold_internal(s, end, 1);
}

long double strtold(const char *restrict s, char **restrict end)
{
	return __strtold_internal(s, end, 2);
}

float strtof_l(const char *restrict s, char **restrict end, locale_t l) { return strtof(s, end); }
double strtod_l(const char *restrict s, char **restrict end, locale_t l) { return strtod(s, end); }
long double strtold_l(const char *restrict s, char **restrict end, locale_t l) { return strtold(s, end); }

double atof(const char *s)
{
	return strtod(s, 0);
}
