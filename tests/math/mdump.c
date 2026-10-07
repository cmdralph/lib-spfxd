/*
 * lib-spfxd — libm regression dump.
 *
 *   mdump <function> <count> [seed] [up|down|zero]
 *
 * Prints "<input-bits> <result-bits>" for count pseudo-random arguments
 * drawn from a distribution suited to the function (wide exponent range
 * plus the region where the function is most used).  Two builds of the
 * library can be compared line by line; differing lines are then judged
 * against mpmath by ulp_check.py --judge.
 */
#include <fenv.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t st = 0x9e3779b97f4a7c15ull;
static uint64_t rnd(void)
{
	st ^= st << 13; st ^= st >> 7; st ^= st << 17;
	return st;
}
static double asd(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }
static uint64_t asu(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
static double unif(double a, double b) { return a + (b - a) * (double)(rnd() >> 11) * 0x1p-53; }
/* random finite double with biased exponent in [lo, hi] */
static double rexp(int lo, int hi, int neg)
{
	uint64_t e = (uint64_t)(lo + (int)(rnd() % (uint64_t)(hi - lo + 1)));
	uint64_t s = neg ? (rnd() & 1) << 63 : 0;
	return asd(s | e << 52 | (rnd() & 0xfffffffffffffull));
}

int main(int argc, char **argv)
{
	if (argc < 3) return 2;
	const char *f = argv[1];
	long n = atol(argv[2]);
	if (argc > 3) st ^= strtoull(argv[3], 0, 0) * 0x2545f4914f6cdd1dull;
	if (argc > 4) fesetround(argv[4][0] == 'u' ? FE_UPWARD : argv[4][0] == 'd' ? FE_DOWNWARD :
	                         argv[4][0] == 'z' ? FE_TOWARDZERO : FE_TONEAREST);
	for (long i = 0; i < n; i++) {
		int mode = (int)(rnd() % 4);
		double x, y = 0, r;
		if (!strcmp(f, "exp")) {
			x = mode ? unif(-745.2, 709.8) : (mode = 0, rexp(0x3c0, 0x408, 1));
			r = exp(x);
		} else if (!strcmp(f, "exp2")) {
			x = mode ? unif(-1075, 1024) : rexp(0x3c0, 0x409, 1);
			r = exp2(x);
		} else if (!strcmp(f, "log") || !strcmp(f, "log2") || !strcmp(f, "log10") || !strcmp(f, "log1p")) {
			if (mode == 0) x = rexp(0, 0x7fe, 0);
			else if (mode == 1) x = 1.0 + unif(-0x1p-6, 0x1p-6);
			else x = unif(0, 1000);
			if (!strcmp(f, "log1p")) x = mode == 1 ? x - 1.0 : x - 0.5;
			r = !strcmp(f, "log") ? log(x) : !strcmp(f, "log2") ? log2(x) : !strcmp(f, "log10") ? log10(x) : log1p(x);
		} else if (!strcmp(f, "sin") || !strcmp(f, "cos") || !strcmp(f, "tan")) {
			x = mode ? unif(-10, 10) : rexp(0x3e0, 0x41e, 1);
			r = f[0] == 's' ? sin(x) : f[0] == 'c' ? cos(x) : tan(x);
		} else if (!strcmp(f, "atan")) {
			x = mode ? unif(-10, 10) : rexp(0x3e0, 0x440, 1);
			r = atan(x);
		} else if (!strcmp(f, "pow")) {
			if (mode == 0) { x = rexp(0x300, 0x4ff, 0); y = unif(-4, 4); }
			else { x = unif(0, 10); y = unif(-20, 20); }
			r = pow(x, y);
		} else if (!strcmp(f, "expf") || !strcmp(f, "logf") || !strcmp(f, "sinf") || !strcmp(f, "cosf") ||
		           !strcmp(f, "powf") || !strcmp(f, "exp2f") || !strcmp(f, "log2f") || !strcmp(f, "tanf") ||
		           !strcmp(f, "atanf")) {
			float xf, yf = 0, rf;
			uint32_t b = (uint32_t)rnd();
			memcpy(&xf, &b, 4);
			if (isnan(xf)) xf = 1.5f;
			if (!strcmp(f, "expf")) { xf = mode ? (float)unif(-104, 89) : xf; rf = expf(xf); }
			else if (!strcmp(f, "exp2f")) { xf = mode ? (float)unif(-150, 128) : xf; rf = exp2f(xf); }
			else if (!strcmp(f, "logf")) { xf = mode ? (float)unif(0, 100) : fabsf(xf); rf = logf(xf); }
			else if (!strcmp(f, "log2f")) { xf = mode ? (float)unif(0, 100) : fabsf(xf); rf = log2f(xf); }
			else if (!strcmp(f, "sinf")) { xf = mode ? (float)unif(-10, 10) : xf; rf = sinf(xf); }
			else if (!strcmp(f, "cosf")) { xf = mode ? (float)unif(-10, 10) : xf; rf = cosf(xf); }
			else if (!strcmp(f, "tanf")) { xf = mode ? (float)unif(-10, 10) : xf; rf = tanf(xf); }
			else if (!strcmp(f, "atanf")) { xf = mode ? (float)unif(-10, 10) : xf; rf = atanf(xf); }
			else { xf = (float)unif(0, 10); yf = (float)unif(-20, 20); rf = powf(xf, yf); }
			uint32_t xb, yb, rb;
			memcpy(&xb, &xf, 4); memcpy(&yb, &yf, 4); memcpy(&rb, &rf, 4);
			printf("%08x %08x %08x\n", xb, yb, rb);
			continue;
		} else {
			fprintf(stderr, "unknown function %s\n", f);
			return 2;
		}
		printf("%016llx %016llx %016llx\n", (unsigned long long)asu(x), (unsigned long long)asu(y),
		       (unsigned long long)asu(r));
	}
	return 0;
}
