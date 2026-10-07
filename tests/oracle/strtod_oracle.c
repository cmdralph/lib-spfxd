/*
 * lib-spfxd oracle test — strtod/strtof/strtold correct rounding.  Parses
 * a deterministic stream of random decimal and hexadecimal strings
 * (long mantissas, extreme exponents, halfway cases) and prints the exact
 * results; must match the host C library.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long long rs = 0xfeedfacecafebeefULL;
static unsigned long long rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }

static void one(const char *s)
{
	char *e1, *e2, *e3;
	errno = 0;
	double d = strtod(s, &e1);
	int er1 = errno;
	errno = 0;
	float f = strtof(s, &e2);
	int er2 = errno;
	errno = 0;
	long double l = strtold(s, &e3);
	int er3 = errno;
	printf("%s -> %a %d %td | %a %d %td | %La %d %td\n", s, d, er1 != 0, e1 - s, (double)f, er2 != 0, e2 - s, l, er3 != 0, e3 - s);
}

int main(void)
{
	static const char *fixed[] = {
		"0", "-0", "1", "0.1", "1e23", "8.98846567431158e307", "2.2250738585072011e-308",
		"2.2250738585072012e-308", "4.9406564584124654e-324", "2.4703282292062327e-324",
		"2.4703282292062328e-324", "1.7976931348623157e308", "1.7976931348623158e308",
		"1.7976931348623159e308", "1e309", "1e-400", "9007199254740993", "9007199254740992.9999999999",
		"0x1p-1074", "0x1.fffffffffffffp1023", "0x1.0000000000000801p0", "0X.8P1", "0x", "0x.p1", ".5", "5.",
		"  +12.5e+2xyz", "-.e1", "inf", "-Infinity", "infinit", "nan", "NAN(abc_123)", "nan(", "1e", "1e+",
		"123456789012345678901234567890e-30", "3.4028235677973366e38", "3.4028234663852886e38",
		"1.401298464324817e-45", "7.006492321624085e-46", "1.18973149535723176502e4932", "3.6451995318824746025e-4951",
		"0.000000000000000000000000000000000000000000000000000000000000000000001e69",
		"179769313486231580793728971405303415079934132710037826936173778980444968292764750946649017977587207096330286416692887910946555547851940402630657488671505820681908902000708383676273854845817711531764475730270069855571366959622842914819860834936475292719074168444365510704342711559699508093042880177904174497791.9999999999",
	};
	for (size_t i = 0; i < sizeof fixed / sizeof *fixed; i++) one(fixed[i]);
	char buf[2048];
	for (int i = 0; i < 20000; i++) {
		int nd = 1 + (int)(rnd() % (i % 10 == 0 ? 800 : 25));
		char *p = buf;
		if (rnd() & 1) *p++ = '-';
		int dot = (int)(rnd() % (unsigned)(nd + 1));
		for (int k = 0; k < nd; k++) {
			if (k == dot) *p++ = '.';
			*p++ = (char)('0' + rnd() % 10);
		}
		int ex = (int)(rnd() % 700) - 350;
		if (i % 7 == 0) ex = (int)(rnd() % 10000) - 5000;
		p += sprintf(p, "e%d", ex);
		one(buf);
	}
	/* exact halfway points between adjacent doubles, plus and minus a tiny bit */
	for (int i = 0; i < 2000; i++) {
		union { unsigned long long u; double d; } x = { rnd() & 0x7fefffffffffffffULL };
		union { unsigned long long u; double d; } y = { x.u + 1 };
		long double mid = ((long double)x.d + (long double)y.d) / 2;
		snprintf(buf, sizeof buf, "%.40Le", mid);
		one(buf);
	}
	for (int i = 0; i < 2000; i++) {
		char *p = buf;
		p += sprintf(p, "0x%llx.%llxp%d", rnd() % 0x100000, rnd(), (int)(rnd() % 2200) - 1100);
		one(buf);
	}
	return 0;
}
