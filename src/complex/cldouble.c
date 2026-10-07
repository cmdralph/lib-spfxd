/* lib-spfxd — complex functions, long double. */
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

#define T long double
#define CT long double _Complex
#define F(n) n##l
#define CF(n) n##l
#define C(x) x##L
#define T_MAX LDBL_MAX
#define T_MIN_SAFE 0x1p-16000L
#define T_SCALE_UP 0x1p200L
#define T_SCALE_DOWN_SQRT 0x1p-100L
#define T_TANH_BIG 23.0L
#define T_BIG 0x1p8000L
#define T_LN2 0.693147180559945309417232121458176568L
#define T_PIO2 1.570796326794896619231321691639751442L
#define T_LOG10E 0.434294481903251827651128918916605082L
#include "complex_impl.h"
