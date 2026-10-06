/* lib-spfxd — <math.h> */
#ifndef _MATH_H
#define _MATH_H
#include <features.h>

__SPFXD_BEGIN_DECLS

#if __FLT_EVAL_METHOD__ == 0 || __FLT_EVAL_METHOD__ == -1
typedef float float_t;
typedef double double_t;
#elif __FLT_EVAL_METHOD__ == 1
typedef double float_t;
typedef double double_t;
#else
typedef long double float_t;
typedef long double double_t;
#endif

#define HUGE_VAL  __builtin_huge_val()
#define HUGE_VALF __builtin_huge_valf()
#define HUGE_VALL __builtin_huge_vall()
#define INFINITY  __builtin_inff()
#define NAN       __builtin_nanf("")

#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4

#define FP_ILOGB0   (-1-0x7fffffff)
#define FP_ILOGBNAN (-1-0x7fffffff)

#define MATH_ERRNO     1
#define MATH_ERREXCEPT 2
#define math_errhandling (MATH_ERRNO | MATH_ERREXCEPT)

#define FP_FAST_FMA_UNDEF

#define fpclassify(x) __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x)
#define isfinite(x)   __builtin_isfinite(x)
#define isinf(x)      __builtin_isinf_sign(x)
#define isnan(x)      __builtin_isnan(x)
#define isnormal(x)   __builtin_isnormal(x)
#define signbit(x)    __builtin_signbit(x)
#define isgreater(x, y)      __builtin_isgreater(x, y)
#define isgreaterequal(x, y) __builtin_isgreaterequal(x, y)
#define isless(x, y)         __builtin_isless(x, y)
#define islessequal(x, y)    __builtin_islessequal(x, y)
#define islessgreater(x, y)  __builtin_islessgreater(x, y)
#define isunordered(x, y)    __builtin_isunordered(x, y)

#define __SPFXD_M1(n) double n(double); float n##f(float); long double n##l(long double);
#define __SPFXD_M2(n) double n(double, double); float n##f(float, float); \
	long double n##l(long double, long double);

__SPFXD_M1(acos) __SPFXD_M1(asin) __SPFXD_M1(atan) __SPFXD_M2(atan2)
__SPFXD_M1(cos) __SPFXD_M1(sin) __SPFXD_M1(tan)
__SPFXD_M1(acosh) __SPFXD_M1(asinh) __SPFXD_M1(atanh)
__SPFXD_M1(cosh) __SPFXD_M1(sinh) __SPFXD_M1(tanh)
__SPFXD_M1(exp) __SPFXD_M1(exp2) __SPFXD_M1(expm1)
__SPFXD_M1(log) __SPFXD_M1(log10) __SPFXD_M1(log1p) __SPFXD_M1(log2) __SPFXD_M1(logb)
__SPFXD_M1(cbrt) __SPFXD_M1(fabs) __SPFXD_M2(hypot) __SPFXD_M2(pow) __SPFXD_M1(sqrt)
__SPFXD_M1(erf) __SPFXD_M1(erfc) __SPFXD_M1(lgamma) __SPFXD_M1(tgamma)
__SPFXD_M1(ceil) __SPFXD_M1(floor) __SPFXD_M1(nearbyint) __SPFXD_M1(rint)
__SPFXD_M1(round) __SPFXD_M1(trunc)
__SPFXD_M2(fmod) __SPFXD_M2(remainder) __SPFXD_M2(copysign) __SPFXD_M2(nextafter)
__SPFXD_M2(fdim) __SPFXD_M2(fmax) __SPFXD_M2(fmin)

double frexp(double, int *);
float frexpf(float, int *);
long double frexpl(long double, int *);
double ldexp(double, int);
float ldexpf(float, int);
long double ldexpl(long double, int);
double modf(double, double *);
float modff(float, float *);
long double modfl(long double, long double *);
double scalbn(double, int);
float scalbnf(float, int);
long double scalbnl(long double, int);
double scalbln(double, long);
float scalblnf(float, long);
long double scalblnl(long double, long);
int ilogb(double);
int ilogbf(float);
int ilogbl(long double);
long lrint(double);
long lrintf(float);
long lrintl(long double);
long long llrint(double);
long long llrintf(float);
long long llrintl(long double);
long lround(double);
long lroundf(float);
long lroundl(long double);
long long llround(double);
long long llroundf(float);
long long llroundl(long double);
double remquo(double, double, int *);
float remquof(float, float, int *);
long double remquol(long double, long double, int *);
double nan(const char *);
float nanf(const char *);
long double nanl(const char *);
double nexttoward(double, long double);
float nexttowardf(float, long double);
long double nexttowardl(long double, long double);
double fma(double, double, double);
float fmaf(float, float, float);
long double fmal(long double, long double, long double);

#if defined(__SPFXD_XSI) || defined(__SPFXD_BSD)
#define M_E        2.7182818284590452354
#define M_LOG2E    1.4426950408889634074
#define M_LOG10E   0.43429448190325182765
#define M_LN2      0.69314718055994530942
#define M_LN10     2.30258509299404568402
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.78539816339744830962
#define M_1_PI     0.31830988618379067154
#define M_2_PI     0.63661977236758134308
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2    1.41421356237309504880
#define M_SQRT1_2  0.70710678118654752440
#define MAXFLOAT   3.40282346638528859812e+38F
extern int signgam;
double j0(double);
double j1(double);
double jn(int, double);
double y0(double);
double y1(double);
double yn(int, double);
#endif

#if defined(__SPFXD_BSD) || defined(__SPFXD_GNU)
#define HUGE 3.40282346638528859812e+38F
double lgamma_r(double, int *);
float lgammaf_r(float, int *);
long double lgammal_r(long double, int *);
double drem(double, double);
float dremf(float, float);
int finite(double);
int finitef(float);
double significand(double);
float significandf(float);
double scalb(double, double);
float scalbf(float, float);
float j0f(float);
float j1f(float);
float jnf(int, float);
float y0f(float);
float y1f(float);
float ynf(int, float);
#endif

#if defined(__SPFXD_GNU)
#define M_El        2.718281828459045235360287471352662498L
#define M_LOG2El    1.442695040888963407359924681001892137L
#define M_LOG10El   0.434294481903251827651128918916605082L
#define M_LN2l      0.693147180559945309417232121458176568L
#define M_LN10l     2.302585092994045684017991454684364208L
#define M_PIl       3.141592653589793238462643383279502884L
#define M_PI_2l     1.570796326794896619231321691639751442L
#define M_PI_4l     0.785398163397448309615660845819875721L
#define M_1_PIl     0.318309886183790671537767526745028724L
#define M_2_PIl     0.636619772367581343075535053490057448L
#define M_2_SQRTPIl 1.128379167095512573896158903121545172L
#define M_SQRT2l    1.414213562373095048801688724209698079L
#define M_SQRT1_2l  0.707106781186547524400844362104849039L
void sincos(double, double *, double *);
void sincosf(float, float *, float *);
void sincosl(long double, long double *, long double *);
__SPFXD_M1(exp10)
double pow10(double);
float pow10f(float);
long double pow10l(long double);
#endif

#undef __SPFXD_M1
#undef __SPFXD_M2
__SPFXD_END_DECLS
#endif
