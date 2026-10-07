/*
 * lib-spfxd — functions that are single AArch64 instructions: correctly
 * rounded square roots (fsqrt) and conversions to integer in the current
 * rounding mode (frintx then fcvtzs: frintx raises inexact like the
 * specification of lrint requires, and the conversion of the now integral
 * value is exact or raises invalid when out of range).
 */
#include <errno.h>
#include <limits.h>
#include <math.h>
#include "fp.h"

double (sqrt)(double x)
{
	double r;
	__asm__ ("fsqrt %d0, %d1" : "=w"(r) : "w"(x));
	if (unlikely(__builtin_isless(x, 0))) errno = EDOM;
	return r;
}

float (sqrtf)(float x)
{
	float r;
	__asm__ ("fsqrt %s0, %s1" : "=w"(r) : "w"(x));
	if (unlikely(__builtin_isless(x, 0))) errno = EDOM;
	return r;
}

long lrint(double x)
{
	double r;
	long n;
	__asm__ ("frintx %d0, %d1" : "=w"(r) : "w"(x));
	__asm__ ("fcvtzs %x0, %d1" : "=r"(n) : "w"(r));
	return n;
}

long lrintf(float x)
{
	float r;
	long n;
	__asm__ ("frintx %s0, %s1" : "=w"(r) : "w"(x));
	__asm__ ("fcvtzs %x0, %s1" : "=r"(n) : "w"(r));
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
