/* lib-spfxd — <tgmath.h>
 *
 * Type-generic macros built on C11 _Generic.  The real type of a call is
 * chosen with the usual rules: any long double argument selects the long
 * double function, otherwise any double or integer argument selects
 * double, otherwise float; a complex argument selects the complex variant.
 */
#ifndef _TGMATH_H
#define _TGMATH_H
#include <math.h>
#include <complex.h>

/* zero of the "generic" type of one argument (integers count as double) */
#define __TG_T(x) _Generic((x), \
	float: (float)0, long double: (long double)0, \
	float _Complex: (float _Complex)0, double _Complex: (double _Complex)0, \
	long double _Complex: (long double _Complex)0, default: (double)0)

#define __TG_R1(fn, x) _Generic(__TG_T(x), \
	float: fn##f, long double: fn##l, default: fn)(x)
#define __TG_RC1(fn, cn, x) _Generic(__TG_T(x), \
	float: fn##f, long double: fn##l, double: fn, \
	float _Complex: cn##f, long double _Complex: cn##l, default: cn)(x)
#define __TG_R2(fn, x, y) _Generic(__TG_T(x) + __TG_T(y), \
	float: fn##f, long double: fn##l, default: fn)(x, y)
#define __TG_RC2(fn, cn, x, y) _Generic(__TG_T(x) + __TG_T(y), \
	float: fn##f, long double: fn##l, double: fn, \
	float _Complex: cn##f, long double _Complex: cn##l, default: cn)(x, y)
#define __TG_R3(fn, x, y, z) _Generic(__TG_T(x) + __TG_T(y) + __TG_T(z), \
	float: fn##f, long double: fn##l, default: fn)(x, y, z)
#define __TG_C1(cn, x) _Generic(__TG_T(x), \
	float: cn##f, float _Complex: cn##f, long double: cn##l, \
	long double _Complex: cn##l, default: cn)(x)

#define acos(x)  __TG_RC1(acos, cacos, x)
#define asin(x)  __TG_RC1(asin, casin, x)
#define atan(x)  __TG_RC1(atan, catan, x)
#define acosh(x) __TG_RC1(acosh, cacosh, x)
#define asinh(x) __TG_RC1(asinh, casinh, x)
#define atanh(x) __TG_RC1(atanh, catanh, x)
#define cos(x)   __TG_RC1(cos, ccos, x)
#define sin(x)   __TG_RC1(sin, csin, x)
#define tan(x)   __TG_RC1(tan, ctan, x)
#define cosh(x)  __TG_RC1(cosh, ccosh, x)
#define sinh(x)  __TG_RC1(sinh, csinh, x)
#define tanh(x)  __TG_RC1(tanh, ctanh, x)
#define exp(x)   __TG_RC1(exp, cexp, x)
#define log(x)   __TG_RC1(log, clog, x)
#define pow(x, y) __TG_RC2(pow, cpow, x, y)
#define sqrt(x)  __TG_RC1(sqrt, csqrt, x)
#define fabs(x)  _Generic(__TG_T(x), float: fabsf, long double: fabsl, double: fabs, \
	float _Complex: cabsf, long double _Complex: cabsl, default: cabs)(x)

#define atan2(x, y)      __TG_R2(atan2, x, y)
#define cbrt(x)          __TG_R1(cbrt, x)
#define ceil(x)          __TG_R1(ceil, x)
#define copysign(x, y)   __TG_R2(copysign, x, y)
#define erf(x)           __TG_R1(erf, x)
#define erfc(x)          __TG_R1(erfc, x)
#define exp2(x)          __TG_R1(exp2, x)
#define expm1(x)         __TG_R1(expm1, x)
#define fdim(x, y)       __TG_R2(fdim, x, y)
#define floor(x)         __TG_R1(floor, x)
#define fma(x, y, z)     __TG_R3(fma, x, y, z)
#define fmax(x, y)       __TG_R2(fmax, x, y)
#define fmin(x, y)       __TG_R2(fmin, x, y)
#define fmod(x, y)       __TG_R2(fmod, x, y)
#define frexp(x, e)      _Generic(__TG_T(x), float: frexpf, long double: frexpl, default: frexp)(x, e)
#define hypot(x, y)      __TG_R2(hypot, x, y)
#define ilogb(x)         __TG_R1(ilogb, x)
#define ldexp(x, e)      _Generic(__TG_T(x), float: ldexpf, long double: ldexpl, default: ldexp)(x, e)
#define lgamma(x)        __TG_R1(lgamma, x)
#define llrint(x)        __TG_R1(llrint, x)
#define llround(x)       __TG_R1(llround, x)
#define log10(x)         __TG_R1(log10, x)
#define log1p(x)         __TG_R1(log1p, x)
#define log2(x)          __TG_R1(log2, x)
#define logb(x)          __TG_R1(logb, x)
#define lrint(x)         __TG_R1(lrint, x)
#define lround(x)        __TG_R1(lround, x)
#define nearbyint(x)     __TG_R1(nearbyint, x)
#define nextafter(x, y)  __TG_R2(nextafter, x, y)
#define nexttoward(x, y) _Generic(__TG_T(x), float: nexttowardf, long double: nexttowardl, default: nexttoward)(x, y)
#define remainder(x, y)  __TG_R2(remainder, x, y)
#define remquo(x, y, q)  _Generic(__TG_T(x) + __TG_T(y), float: remquof, long double: remquol, default: remquo)(x, y, q)
#define rint(x)          __TG_R1(rint, x)
#define round(x)         __TG_R1(round, x)
#define scalbn(x, n)     _Generic(__TG_T(x), float: scalbnf, long double: scalbnl, default: scalbn)(x, n)
#define scalbln(x, n)    _Generic(__TG_T(x), float: scalblnf, long double: scalblnl, default: scalbln)(x, n)
#define tgamma(x)        __TG_R1(tgamma, x)
#define trunc(x)         __TG_R1(trunc, x)

#undef creal
#undef cimag
#define carg(x)  __TG_C1(carg, x)
#define cimag(x) __TG_C1(cimag, x)
#define conj(x)  __TG_C1(conj, x)
#define cproj(x) __TG_C1(cproj, x)
#define creal(x) __TG_C1(creal, x)

#endif
