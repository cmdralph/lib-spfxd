/*
 * lib-spfxd — functions that are single RISC-V instructions: correctly
 * rounded square roots (fsqrt) and conversions to integer in the current
 * rounding mode (fcvt with the dynamic rounding mode raises inexact like
 * lrint must, and invalid when the value is out of range).
 */
#include <errno.h>
#include <math.h>
#include "fp.h"

double (sqrt)(double x)
{
	double r;
	__asm__ ("fsqrt.d %0, %1" : "=f"(r) : "f"(x));
	if (unlikely(__builtin_isless(x, 0))) errno = EDOM;
	return r;
}

float (sqrtf)(float x)
{
	float r;
	__asm__ ("fsqrt.s %0, %1" : "=f"(r) : "f"(x));
	if (unlikely(__builtin_isless(x, 0))) errno = EDOM;
	return r;
}

long lrint(double x)
{
	long n;
	__asm__ __volatile__ ("fcvt.l.d %0, %1, dyn" : "=r"(n) : "f"(x));
	return n;
}

long lrintf(float x)
{
	long n;
	__asm__ __volatile__ ("fcvt.l.s %0, %1, dyn" : "=r"(n) : "f"(x));
	return n;
}

long long llrint(double x)
{
	return lrint(x);
}

long long llrintf(float x)
{
	return lrintf(x);
}
