/*
 * lib-spfxd — libm error reporting.
 *
 * math_errhandling is MATH_ERRNO | MATH_ERREXCEPT: every domain, pole and
 * range error both sets errno and raises the IEEE exception.  Exceptions
 * are raised by performing the offending operation on values the compiler
 * cannot see (fp_barrier), so unmasked traps fire exactly as they would
 * for a naive implementation.
 */
#include <errno.h>
#include <float.h>
#include "fp.h"

double __math_invalid(double x)
{
	double y = (x - x) / (x - x);
	if (!__builtin_isnan(x)) errno = EDOM;
	return y;
}

double __math_divzero(uint32_t sign)
{
	double y = fp_barrier(sign ? -1.0 : 1.0) / 0.0;
	errno = ERANGE;
	return y;
}

double __math_oflow(uint32_t sign)
{
	double y = fp_barrier(sign ? -0x1p769 : 0x1p769) * 0x1p769;
	errno = ERANGE;
	return y;
}

double __math_uflow(uint32_t sign)
{
	double y = fp_barrier(sign ? -0x1p-767 : 0x1p-767) * 0x1p-767;
	errno = ERANGE;
	return y;
}

double __math_check_oflow(double y)
{
	if (__builtin_isinf(y)) errno = ERANGE;
	return y;
}

double __math_check_uflow(double y)
{
	if (__builtin_fabs(y) < DBL_MIN) errno = ERANGE;
	return y;
}

double __math_range(double y)
{
	if (__builtin_isinf(y) || __builtin_fabs(y) < DBL_MIN) errno = ERANGE;
	return y;
}

float __math_invalidf(float x)
{
	float y = (x - x) / (x - x);
	if (!__builtin_isnan(x)) errno = EDOM;
	return y;
}

float __math_divzerof(uint32_t sign)
{
	float y = fp_barrierf(sign ? -1.0f : 1.0f) / 0.0f;
	errno = ERANGE;
	return y;
}

float __math_oflowf(uint32_t sign)
{
	float y = fp_barrierf(sign ? -0x1p97f : 0x1p97f) * 0x1p97f;
	errno = ERANGE;
	return y;
}

float __math_uflowf(uint32_t sign)
{
	float y = fp_barrierf(sign ? -0x1p-95f : 0x1p-95f) * 0x1p-95f;
	errno = ERANGE;
	return y;
}

/* Round a double result to float, reporting overflow and underflow that
 * happen in the conversion itself.  Zero and infinite inputs are taken to
 * be exact results and are not errors. */
float __math_narrowf(double y)
{
	float r = (float)y;
	if (__builtin_isinf(r) && !__builtin_isinf(y)) errno = ERANGE;
	else if (__builtin_fabsf(r) < FLT_MIN && y != 0) errno = ERANGE;
	return r;
}

long double __math_invalidl(long double x)
{
	long double y = (x - x) / (x - x);
	if (!__builtin_isnan(x)) errno = EDOM;
	return y;
}

long double __math_divzerol(uint32_t sign)
{
	long double y = fp_barrierl(sign ? -1.0L : 1.0L) / 0.0L;
	errno = ERANGE;
	return y;
}

long double __math_oflowl(uint32_t sign)
{
	long double y = fp_barrierl(sign ? -0x1p10000L : 0x1p10000L) * 0x1p10000L;
	errno = ERANGE;
	return y;
}

long double __math_uflowl(uint32_t sign)
{
	long double y = fp_barrierl(sign ? -0x1p-10000L : 0x1p-10000L) * 0x1p-10000L;
	errno = ERANGE;
	return y;
}

long double __math_rangel(long double y)
{
	if (__builtin_isinf(y) || __builtin_fabsl(y) < LDBL_MIN) errno = ERANGE;
	return y;
}
