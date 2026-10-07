/*
 * lib-spfxd oracle test — strtod on 400,000 generated inputs aimed at the
 * Eisel-Lemire fast path and its fallbacks: random significands of 1..19
 * digits over the whole exponent range, %.17g/%.16g/%.15g renderings of
 * random doubles, exact binary midpoints written as integers (round to
 * even), and values at the overflow and subnormal thresholds.  Both
 * libraries are correctly rounded, so the result bits must match.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t st = 0x2545f4914f6cdd1dULL;
static uint64_t rnd(void)
{
	st ^= st << 13;
	st ^= st >> 7;
	st ^= st << 17;
	return st;
}

static unsigned long long h;
static void check(const char *s)
{
	char *end;
	double d = strtod(s, &end);
	uint64_t u;
	memcpy(&u, &d, 8);
	h = h * 1000003 ^ u ^ (unsigned long long)(end - s);
	/* print a sample so a mismatch can be located */
	if ((rnd() & 1023) == 0) printf("%s -> %016llx\n", s, (unsigned long long)u);
}

int main(void)
{
	char buf[128];
	for (int i = 0; i < 150000; i++) {
		int nd = 1 + (int)(rnd() % 19);
		uint64_t m = 0;
		for (int k = 0; k < nd; k++) m = m * 10 + rnd() % 10;
		int e = (int)(rnd() % 700) - 360;
		snprintf(buf, sizeof buf, "%s%llue%d", rnd() & 1 ? "-" : "", (unsigned long long)m, e);
		check(buf);
	}
	for (int i = 0; i < 150000; i++) {
		uint64_t u = rnd() & 0x7fffffffffffffffULL;
		if ((u >> 52) == 0x7ff) u ^= 1ULL << 62;
		double d;
		memcpy(&d, &u, 8);
		static const char *f[] = { "%.17g", "%.16g", "%.15g", "%.18g" };
		snprintf(buf, sizeof buf, f[i & 3], d);
		check(buf);
	}
	for (int i = 0; i < 80000; i++) {
		/* N = m 2^k + 2^(k-1) with a 53-bit m: exactly halfway between two
		 * doubles, written in at most 19 digits */
		uint64_t m = (rnd() >> 11) | (1ULL << 52);
		int k = 1 + (int)(rnd() % 10);
		uint64_t n = (m << k) + (1ULL << (k - 1));
		if (n >= 10000000000000000000ULL) n >>= 1;
		snprintf(buf, sizeof buf, "%llu", (unsigned long long)n);
		check(buf);
		snprintf(buf, sizeof buf, "%llu.0e%d", (unsigned long long)n, (int)(rnd() % 7));
		check(buf);
	}
	static const char *edge[] = {
		"1.7976931348623157e308", "1.7976931348623158e308", "1.7976931348623159e308",
		"2.2250738585072014e-308", "2.2250738585072011e-308", "2.2250738585072012e-308",
		"4.9406564584124654e-324", "2.4703282292062327e-324", "2.4703282292062328e-324",
		"1e-342", "1e-343", "9e308", "1e309", "9007199254740993", "9007199254740995",
		"123456789012345678e-3", "1e23", "8.5e-5", "0.1", "0.3", "1e22", "1e-22",
	};
	for (size_t i = 0; i < sizeof edge / sizeof *edge; i++) {
		check(edge[i]);
		char *end;
		printf("%s -> %a\n", edge[i], strtod(edge[i], &end));
	}
	printf("hash %016llx\n", h);
	return 0;
}
