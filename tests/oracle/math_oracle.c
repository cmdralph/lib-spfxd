/*
 * lib-spfxd oracle test — libm special cases (C11 Annex F): results for
 * zeros, infinities, NaNs, poles and domain edges, plus exact cases,
 * together with errno and the raised exception flags.  These are fully
 * specified, so they must match the host library exactly.  Inexact finite
 * results are printed to 13 significant digits only: their last-bit
 * rounding is checked against mpmath by tests/math, and the host library
 * is not always correctly rounded.
 */
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

#define FE (FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW)

static const char *val(double r, char *buf)
{
	if (isnan(r)) snprintf(buf, 64, "nan");          /* NaN sign is unspecified */
	else if (isinf(r) || r == 0 || (fabs(r) < 0x1p53 && r == nearbyint(r))) snprintf(buf, 64, "%a", r);
	else snprintf(buf, 64, "~%.13g", r);
	return buf;
}

static void show(const char *name, double x, double r)
{
	int ex = fetestexcept(FE), e = errno;
	char b[64];
	printf("%-10s %-8a -> %-24s errno=%d fe=%#x\n", name, x, val(r, b), e, ex);
}

#define T1(f, x) do { errno = 0; feclearexcept(FE_ALL_EXCEPT); volatile double _r = f(x); show(#f, x, _r); } while (0)
#define T2(f, x, y) do { errno = 0; feclearexcept(FE_ALL_EXCEPT); volatile double _r = f(x, y); double _x = (x), _y = (y); \
	int _e = errno, _f = fetestexcept(FE); char _b[64]; \
	printf("%-10s %a %a -> %s errno=%d fe=%#x\n", #f, _x, _y, val(_r, _b), _e, _f); } while (0)

int main(void)
{
	const double sp[] = { 0.0, -0.0, INFINITY, -INFINITY, NAN, 1.0, -1.0, 1e-310, DBL_MAX, -DBL_MAX, 710.0, -746.0 };
	for (unsigned i = 0; i < sizeof sp / sizeof *sp; i++) {
		double x = sp[i];
		T1(exp, x); T1(exp2, x); T1(expm1, x); T1(log, x); T1(log2, x); T1(log10, x); T1(log1p, x);
		T1(sqrt, x); T1(cbrt, x); T1(sin, x); T1(cos, x); T1(tan, x); T1(asin, x); T1(acos, x); T1(atan, x);
		T1(sinh, x); T1(cosh, x); T1(tanh, x); T1(asinh, x); T1(acosh, x); T1(atanh, x); T1(erf, x); T1(erfc, x);
		T1(tgamma, x); T1(lgamma, x); T1(floor, x); T1(ceil, x); T1(round, x); T1(trunc, x); T1(rint, x);
		T1(logb, x); T1(fabs, x);
	}
	/* exact results */
	T1(sqrt, 4.0); T1(exp2, 10.0); T1(log2, 1024.0); T1(log10, 1000.0); T1(cbrt, 27.0); T1(cbrt, -8.0);
	T1(exp, 1.0); T1(log, M_E); T1(sin, M_PI); T1(cos, M_PI); T1(atan, 1.0); T1(tgamma, 5.0); T1(lgamma, 2.0);
	const double pw[][2] = { { 0, -1 }, { -0.0, -1 }, { -0.0, -2 }, { 0, 0 }, { NAN, 0 }, { 1, NAN }, { -1, INFINITY },
	                         { 2, INFINITY }, { 0.5, INFINITY }, { 2, -INFINITY }, { -INFINITY, 3 }, { -INFINITY, -3 },
	                         { -INFINITY, 2 }, { INFINITY, -1 }, { -8, 1.0 / 3 }, { -2, 3 }, { 2, 0.5 }, { 10, 308 },
	                         { 10, 309 }, { 10, -330 }, { 2, -1074 }, { 2, -1075 }, { -1, 1e300 }, { 1e-300, -2 } };
	for (unsigned i = 0; i < sizeof pw / sizeof *pw; i++) T2(pow, pw[i][0], pw[i][1]);
	const double a2[][2] = { { 0, 0 }, { -0.0, 0 }, { 0, -0.0 }, { -0.0, -0.0 }, { 1, 0 }, { -1, -0.0 },
	                         { INFINITY, INFINITY }, { INFINITY, -INFINITY }, { -INFINITY, 1 }, { 1, -INFINITY } };
	for (unsigned i = 0; i < sizeof a2 / sizeof *a2; i++) T2(atan2, a2[i][0], a2[i][1]);
	T2(hypot, INFINITY, NAN); T2(hypot, NAN, -INFINITY); T2(hypot, 3, 4); T2(hypot, DBL_MAX, DBL_MAX);
	T2(fmod, 5.5, 2); T2(fmod, -5.5, 2); T2(fmod, 1, 0); T2(fmod, INFINITY, 1); T2(fmod, 1, INFINITY);
	T2(remainder, 5, 2); T2(remainder, 7, 2); T2(remainder, 1, 0); T2(remainder, -0.0, 1);
	T2(fmax, NAN, 1); T2(fmin, 1, NAN); T2(fdim, 5, 3); T2(fdim, 3, 5); T2(copysign, 1, -0.0);
	T2(nextafter, 0, 1); T2(nextafter, DBL_MAX, INFINITY); T2(nextafter, 1, 1); T2(ldexp, 1, 2000); T2(ldexp, 1, -2000);
	T2(scalbn, 3, 4);
	int q;
	double rq = remquo(10.0, 3.0, &q);
	printf("remquo %a %d\n", rq, q & 7);
	int e;
	double m = frexp(48.0, &e);
	printf("frexp %a %d ilogb %d %d %d\n", m, e, ilogb(48.0), ilogb(0.0) == FP_ILOGB0, ilogb(NAN) == FP_ILOGBNAN);
	double ip;
	printf("modf %a %a\n", modf(-3.75, &ip), ip);
	printf("lround %ld %ld llrint %lld\n", lround(2.5), lround(-2.5), llrint(2.5));
	printf("fma %a %a\n", fma(0x1p-1074, 0.5, 0), fma(1 + 0x1p-52, 1 + 0x1p-52, -1));
	printf("nan %d %d\n", isnan(nan("")), signbit(-0.0) != 0);
	printf("float: %a %a %a %a\n", (double)expf(-104.0f), (double)logf(0.0f), (double)sqrtf(-1.0f), (double)powf(-0.0f, -3.0f));
	printf("long double: %La %d %La %La\n", expl(1.0L), isnan(logl(-1.0L)), powl(2.0L, 0.5L), sqrtl(2.0L));
	return 0;
}
