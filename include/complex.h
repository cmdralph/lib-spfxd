/* lib-spfxd — <complex.h> */
#ifndef _COMPLEX_H
#define _COMPLEX_H
#include <features.h>

__SPFXD_BEGIN_DECLS

#define complex _Complex
#ifdef __GNUC__
#define _Complex_I (__extension__ 1.0iF)
#else
#define _Complex_I 1.0iF
#endif
#define I _Complex_I

#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ + __GNUC__ * 100 >= 407)
#define CMPLX(x, y)  __builtin_complex((double)(x), (double)(y))
#define CMPLXF(x, y) __builtin_complex((float)(x), (float)(y))
#define CMPLXL(x, y) __builtin_complex((long double)(x), (long double)(y))
#endif

#define __SPFXD_C1(n) double _Complex n(double _Complex); \
	float _Complex n##f(float _Complex); \
	long double _Complex n##l(long double _Complex);
#define __SPFXD_CR(n) double n(double _Complex); float n##f(float _Complex); \
	long double n##l(long double _Complex);

__SPFXD_C1(cacos) __SPFXD_C1(casin) __SPFXD_C1(catan)
__SPFXD_C1(ccos) __SPFXD_C1(csin) __SPFXD_C1(ctan)
__SPFXD_C1(cacosh) __SPFXD_C1(casinh) __SPFXD_C1(catanh)
__SPFXD_C1(ccosh) __SPFXD_C1(csinh) __SPFXD_C1(ctanh)
__SPFXD_C1(cexp) __SPFXD_C1(clog) __SPFXD_C1(csqrt)
__SPFXD_C1(conj) __SPFXD_C1(cproj)
__SPFXD_CR(cabs) __SPFXD_CR(carg) __SPFXD_CR(cimag) __SPFXD_CR(creal)

double _Complex cpow(double _Complex, double _Complex);
float _Complex cpowf(float _Complex, float _Complex);
long double _Complex cpowl(long double _Complex, long double _Complex);

#ifdef __SPFXD_GNU
__SPFXD_C1(clog10)
#endif

#undef __SPFXD_C1
#undef __SPFXD_CR

#ifdef __GNUC__
#define creal(x) __real__(x)
#define cimag(x) __imag__(x)
#define crealf(x) __real__(x)
#define cimagf(x) __imag__(x)
#define creall(x) __real__(x)
#define cimagl(x) __imag__(x)
#endif

__SPFXD_END_DECLS
#endif
