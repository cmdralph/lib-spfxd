/*
 * lib-spfxd — functions that map to single SSE instructions.
 *
 * sqrtsd/sqrtss are correctly rounded in the current rounding mode;
 * cvtsd2si/cvtss2si round in the current rounding mode as lrint requires
 * and raise invalid (returning LONG_MIN) when the result does not fit.
 */
#include <errno.h>
#include <math.h>
#include "fp.h"

double sqrt(double x)
{
	if (x < 0) return __math_invalid(x);
	double r;
	__asm__ ("sqrtsd %1, %0" : "=x"(r) : "x"(x));
	return r;
}

float sqrtf(float x)
{
	if (x < 0) return __math_invalidf(x);
	float r;
	__asm__ ("sqrtss %1, %0" : "=x"(r) : "x"(x));
	return r;
}

long lrint(double x)
{
	long r;
	__asm__ ("cvtsd2si %1, %0" : "=r"(r) : "x"(x));
	return r;
}

long lrintf(float x)
{
	long r;
	__asm__ ("cvtss2si %1, %0" : "=r"(r) : "x"(x));
	return r;
}

long long llrint(double x)
{
	return lrint(x);
}

long long llrintf(float x)
{
	return lrintf(x);
}
