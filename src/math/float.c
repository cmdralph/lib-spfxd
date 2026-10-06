/*
 * lib-spfxd — float transcendental functions.
 *
 * Each is evaluated by the double implementation on the exactly converted
 * argument and rounded once to float.  The double results are within ~0.5
 * ulp of double, i.e. ~2^-29 ulp of float, so the float results are
 * correctly rounded except in very rare double-rounding cases.  Range
 * errors that only appear in the conversion to float are reported by
 * __math_narrowf.
 */
#include <math.h>
#include "fp.h"

#define F1(name) float name##f(float x) { return __math_narrowf(name(x)); }
#define F1X(name) float name##f(float x) { return (float)name(x); }
#define F2(name) float name##f(float x, float y) { return __math_narrowf(name(x, y)); }

/* results bounded in magnitude: no overflow, underflow only for tiny x
 * where the double result is x itself */
F1(sin) F1(tan) F1(asin) F1(atan) F1(sinh) F1(tanh) F1(asinh) F1(atanh)
F1(erf) F1(expm1) F1(log1p) F1(cbrt)
F1X(cos) F1X(acos) F1X(acosh) F1X(log) F1X(log2) F1X(log10)
F1(exp) F1(exp2) F1(exp10) F1(cosh) F1(tgamma) F1(lgamma)
F2(atan2) F2(pow) F2(hypot)

float erfcf(float x) { return __math_narrowf(erfc(x)); }
float pow10f(float x) { return exp10f(x); }
float gammaf(float x) { return lgammaf(x); }

float lgammaf_r(float x, int *sg)
{
	return __math_narrowf(__lgamma_r(x, sg));
}

void sincosf(float x, float *s, float *c)
{
	double sd, cd;
	sincos(x, &sd, &cd);
	*s = __math_narrowf(sd);
	*c = (float)cd;
}
