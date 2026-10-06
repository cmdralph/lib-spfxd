/*
 * lib-spfxd — libm accuracy driver.
 *
 * Evaluates one function on a deterministic set of arguments (special
 * values, then pseudo-random values spread over the given ranges) and
 * prints one line per evaluation:
 *     <x as %a> [<y as %a>] <result as %a> <errno>
 * tests/math/ulp_check.py recomputes each result with mpmath and reports
 * the maximum error in ulps.
 *
 *   ulp_driver <func> <count> <lo> <hi> [lo2 hi2]
 */
#define _GNU_SOURCE
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef double (*f1)(double);
typedef double (*f2)(double, double);
typedef float (*g1)(float);
typedef long double (*l1)(long double);
typedef long double (*l2)(long double, long double);

static double lgam(double x) { return lgamma(x); }
static double jn3(double x) { return jn(3, x); }
static double yn3(double x) { return yn(3, x); }
static double jn20(double x) { return jn(20, x); }
static double fma_xy(double x, double y) { return fma(x, y, -x * y); }

static const struct { const char *name; f1 f; } F1[] = {
	{ "exp", exp }, { "exp2", exp2 }, { "exp10", exp10 }, { "expm1", expm1 },
	{ "log", log }, { "log2", log2 }, { "log10", log10 }, { "log1p", log1p },
	{ "sin", sin }, { "cos", cos }, { "tan", tan },
	{ "asin", asin }, { "acos", acos }, { "atan", atan },
	{ "sinh", sinh }, { "cosh", cosh }, { "tanh", tanh },
	{ "asinh", asinh }, { "acosh", acosh }, { "atanh", atanh },
	{ "cbrt", cbrt }, { "sqrt", sqrt }, { "erf", erf }, { "erfc", erfc },
	{ "lgamma", lgam }, { "tgamma", tgamma },
	{ "j0", j0 }, { "j1", j1 }, { "y0", y0 }, { "y1", y1 },
	{ "jn3", jn3 }, { "yn3", yn3 }, { "jn20", jn20 },
};
static const struct { const char *name; f2 f; } F2[] = {
	{ "pow", pow }, { "atan2", atan2 }, { "hypot", hypot }, { "fmod", fmod },
	{ "remainder", remainder }, { "fma_xy", fma_xy },
};
static const struct { const char *name; g1 f; } G1[] = {
	{ "expf", expf }, { "logf", logf }, { "sinf", sinf }, { "cosf", cosf },
	{ "tanf", tanf }, { "powf2", 0 },
};
static const struct { const char *name; l1 f; } L1[] = {
	{ "expl", expl }, { "exp2l", exp2l }, { "expm1l", expm1l }, { "logl", logl },
	{ "log2l", log2l }, { "log10l", log10l }, { "log1pl", log1pl },
	{ "sinl", sinl }, { "cosl", cosl }, { "tanl", tanl },
	{ "asinl", asinl }, { "acosl", acosl }, { "atanl", atanl },
	{ "sinhl", sinhl }, { "coshl", coshl }, { "tanhl", tanhl },
	{ "asinhl", asinhl }, { "acoshl", acoshl }, { "atanhl", atanhl },
	{ "cbrtl", cbrtl }, { "sqrtl", sqrtl },
};
static const struct { const char *name; l2 f; } L2[] = {
	{ "powl", powl }, { "atan2l", atan2l }, { "hypotl", hypotl }, { "fmodl", fmodl },
};

static unsigned long long rng = 0x9e3779b97f4a7c15ULL;
static double urand(void)
{
	rng ^= rng << 13;
	rng ^= rng >> 7;
	rng ^= rng << 17;
	return (rng >> 11) * 0x1p-53;
}

/* Spread samples: uniform for narrow ranges, log-uniform in |x| for
 * ranges spanning many binades. */
static double sample(double lo, double hi)
{
	if (lo > 0 && hi / lo > 64) return exp(log(lo) + urand() * (log(hi) - log(lo)));
	if (hi < 0 && lo / hi > 64) return -exp(log(-hi) + urand() * (log(-lo) - log(-hi)));
	return lo + urand() * (hi - lo);
}

int main(int argc, char **argv)
{
	if (argc < 5) {
		fprintf(stderr, "usage: %s func count lo hi [lo2 hi2]\n", argv[0]);
		return 2;
	}
	const char *fn = argv[1];
	int n = atoi(argv[2]);
	double lo = strtod(argv[3], 0), hi = strtod(argv[4], 0);
	double lo2 = argc > 6 ? strtod(argv[5], 0) : lo, hi2 = argc > 6 ? strtod(argv[6], 0) : hi;
	if (argc > 7) rng ^= strtoull(argv[7], 0, 0);

	for (size_t i = 0; i < sizeof F1 / sizeof *F1; i++) if (!strcmp(fn, F1[i].name)) {
		for (int k = 0; k < n; k++) {
			double x = sample(lo, hi);
			errno = 0;
			double r = F1[i].f(x);
			printf("%a %a %d\n", x, r, errno);
		}
		return 0;
	}
	for (size_t i = 0; i < sizeof F2 / sizeof *F2; i++) if (!strcmp(fn, F2[i].name)) {
		for (int k = 0; k < n; k++) {
			double x = sample(lo, hi), y = sample(lo2, hi2);
			errno = 0;
			double r = F2[i].f(x, y);
			printf("%a %a %a %d\n", x, y, r, errno);
		}
		return 0;
	}
	for (size_t i = 0; i < sizeof G1 / sizeof *G1; i++) if (!strcmp(fn, G1[i].name) && G1[i].f) {
		for (int k = 0; k < n; k++) {
			float x = (float)sample(lo, hi);
			errno = 0;
			float r = G1[i].f(x);
			printf("%a %a %d\n", (double)x, (double)r, errno);
		}
		return 0;
	}
	for (size_t i = 0; i < sizeof L1 / sizeof *L1; i++) if (!strcmp(fn, L1[i].name)) {
		for (int k = 0; k < n; k++) {
			long double x = (long double)sample(lo, hi) * (1.0L + (urand() - 0.5) * 0x1p-52L);
			errno = 0;
			long double r = L1[i].f(x);
			printf("%La %La %d\n", x, r, errno);
		}
		return 0;
	}
	for (size_t i = 0; i < sizeof L2 / sizeof *L2; i++) if (!strcmp(fn, L2[i].name)) {
		for (int k = 0; k < n; k++) {
			long double x = (long double)sample(lo, hi) * (1.0L + (urand() - 0.5) * 0x1p-52L);
			long double y = (long double)sample(lo2, hi2) * (1.0L + (urand() - 0.5) * 0x1p-52L);
			errno = 0;
			long double r = L2[i].f(x, y);
			printf("%La %La %La %d\n", x, y, r, errno);
		}
		return 0;
	}
	fprintf(stderr, "unknown function %s\n", fn);
	return 2;
}
