/* lib-spfxd — complex functions, double and float. */
#include <complex.h>
#include <float.h>
#include <math.h>
#include "fp.h"

#undef creal
#undef cimag
#undef crealf
#undef cimagf
#undef creall
#undef cimagl

#define T double
#define CT double _Complex
#define F(n) n
#define CF(n) n
#define C(x) x
#define T_MAX DBL_MAX
#define T_MIN_SAFE 0x1p-1000
#define T_SCALE_UP 0x1p200
#define T_SCALE_DOWN_SQRT 0x1p-100
#define T_TANH_BIG 22.0
#define T_BIG 0x1p500
#define T_LN2 0x1.62e42fefa39efp-1
#define T_PIO2 0x1.921fb54442d18p0
#define T_LOG10E 0x1.bcb7b1526e50ep-2
#include "complex_impl.h"

/* float versions: computed in double and rounded once */
static always_inline float _Complex mkf(double _Complex z)
{
	float _Complex r;
	__real__ r = (float)__real__ z;
	__imag__ r = (float)__imag__ z;
	return r;
}

#define FW(n) float _Complex n##f(float _Complex z) { return mkf(n((double _Complex)z)); }
FW(cacos) FW(casin) FW(catan) FW(ccos) FW(csin) FW(ctan)
FW(cacosh) FW(casinh) FW(catanh) FW(ccosh) FW(csinh) FW(ctanh)
FW(cexp) FW(clog) FW(csqrt) FW(cproj) FW(clog10)

float _Complex conjf(float _Complex z) { float _Complex r = z; __imag__ r = -__imag__ z; return r; }
float cabsf(float _Complex z) { return (float)hypot(__real__ z, __imag__ z); }
float cargf(float _Complex z) { return (float)atan2(__imag__ z, __real__ z); }
float crealf(float _Complex z) { return __real__ z; }
float cimagf(float _Complex z) { return __imag__ z; }
float _Complex cpowf(float _Complex z, float _Complex w)
{
	return mkf(cpow((double _Complex)z, (double _Complex)w));
}
