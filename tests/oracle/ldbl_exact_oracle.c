/*
 * lib-spfxd oracle test — the exactly specified long double functions on
 * random bit patterns (normal, subnormal, near-integer, huge), in all
 * four rounding modes where the result depends on the mode: fmal, sqrtl,
 * fmodl, remquol, rintl, nearbyintl, floorl, ceill, truncl, roundl,
 * lrintl, nextafterl, frexpl, ldexpl, ilogbl, logbl, modfl.  Every result
 * is correctly rounded (or exact), so both libraries must agree bit for
 * bit.  Works for both long double formats (x87 extended, binary128).
 */
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint64_t st = 0x9e3779b97f4a7c15ULL;
static uint64_t rnd(void)
{
	st ^= st << 13;
	st ^= st >> 7;
	st ^= st << 17;
	return st;
}

/* a random finite long double with exponent in [elo, ehi] (biased) */
static long double gen(int elo, int ehi)
{
	long double x;
	memset(&x, 0, sizeof x);
	int e = elo + (int)(rnd() % (unsigned)(ehi - elo + 1));
	int neg = rnd() & 1;
#if LDBL_MANT_DIG == 64
	struct { uint64_t m; uint16_t se; } u;
	u.m = rnd();
	if (rnd() % 4 == 0) u.m &= ~0ULL << (rnd() % 64);      /* short significand */
	if (e) u.m |= 1ULL << 63;
	else u.m &= ~(1ULL << 63);
	u.se = (uint16_t)(e | neg << 15);
	memcpy(&x, &u, 10);
#else
	unsigned __int128 m = ((unsigned __int128)rnd() << 64 | rnd()) & ((((unsigned __int128)1) << 112) - 1);
	if (rnd() % 4 == 0) m &= ~(unsigned __int128)0 << (rnd() % 112);
	m |= (unsigned __int128)(e | neg << 15) << 112;
	memcpy(&x, &m, 16);
#endif
	return x;
}

static long double any(void)
{
	switch (rnd() % 8) {
	case 0: return gen(0, 0);                        /* subnormal */
	case 1: return gen(1, 200);                      /* tiny */
	case 2: return gen(0x7fff - 200, 0x7ffe);        /* huge */
	case 3: return gen(0x3fff - 4, 0x3fff + LDBL_MANT_DIG + 2);   /* near integers */
	default: return gen(1, 0x7ffe);
	}
}

static void p(const char *what, long double r)
{
	if (isnan(r)) printf("%s nan\n", what);
	else printf("%s %La\n", what, r);
}

static const int modes[] = { FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO };

int main(void)
{
	char b[64];
	for (int i = 0; i < 4000; i++) {
		long double x = any(), y = any(), z = any();
		if (rnd() % 3 == 0) y = gen(0x3fff - 30, 0x3fff + 30);
		if (rnd() % 4 == 0) {
			/* x y close to -z: cancellation */
			z = -(x * y) * (1 + (rnd() % 3 - 1) * LDBL_EPSILON);
			if (!isfinite(z)) z = any();
		}
		int q;
		for (int k = 0; k < 4; k++) {
			fesetround(modes[k]);
			snprintf(b, sizeof b, "%d fma%d", i, k); p(b, fmal(x, y, z));
			snprintf(b, sizeof b, "%d sqrt%d", i, k); p(b, sqrtl(fabsl(x)));
			snprintf(b, sizeof b, "%d rint%d", i, k); p(b, rintl(x));
			snprintf(b, sizeof b, "%d nbi%d", i, k); p(b, nearbyintl(y));
			if (fabsl(y) < 0x1p62L) printf("%d lrint%d %ld\n", i, k, lrintl(y));
		}
		fesetround(FE_TONEAREST);
		snprintf(b, sizeof b, "%d fmod", i); p(b, fmodl(x, y));
		long double r = remquol(x, y, &q);
		snprintf(b, sizeof b, "%d remquo %d", i, isnan(r) ? 0 : q & 7);   /* quo unspecified for NaN */
		p(b, r);
		snprintf(b, sizeof b, "%d floor", i); p(b, floorl(x));
		snprintf(b, sizeof b, "%d ceil", i); p(b, ceill(x));
		snprintf(b, sizeof b, "%d trunc", i); p(b, truncl(x));
		snprintf(b, sizeof b, "%d round", i); p(b, roundl(x));
		snprintf(b, sizeof b, "%d next", i); p(b, nextafterl(x, y));
		int e;
		long double f = frexpl(x, &e);
		snprintf(b, sizeof b, "%d frexp %d", i, e); p(b, f);
		snprintf(b, sizeof b, "%d ldexp", i); p(b, ldexpl(x, (int)(rnd() % 400) - 200));
		printf("%d ilogb %d\n", i, ilogbl(x));
		snprintf(b, sizeof b, "%d logb", i); p(b, logbl(x));
		long double ip;
		f = modfl(x, &ip);
		snprintf(b, sizeof b, "%d modf", i); p(b, f);
		p(b, ip);
	}
	return 0;
}
