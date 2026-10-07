/*
 * lib-spfxd — exponent/sign manipulation and comparison functions.
 *
 * Everything here is exact; the only floating-point operations are those
 * needed to raise the IEEE exceptions C requires (overflow and underflow in
 * scalbn/nextafter, invalid on signaling NaNs).
 */
#include <errno.h>
#include <fenv.h>
#include <limits.h>
#include <math.h>
#include "fp.h"

/* ---------------------------------------------------------------- double */

double (copysign)(double x, double y)
{
	return asdouble((asuint64(x) & ~(1ULL << 63)) | (asuint64(y) & (1ULL << 63)));
}

double (fabs)(double x)
{
	return asdouble(asuint64(x) & ~(1ULL << 63));
}

double frexp(double x, int *e)
{
	uint64_t u = asuint64(x);
	int ee = (int)(u >> 52 & 0x7ff);
	if (!ee) {
		if (x != 0) {
			x = frexp(x * 0x1p64, e);
			*e -= 64;
		} else {
			*e = 0;
		}
		return x;
	}
	if (ee == 0x7ff) {
		*e = 0;
		return x + x;
	}
	*e = ee - 0x3fe;
	u = (u & 0x800fffffffffffffULL) | 0x3fe0000000000000ULL;
	return asdouble(u);
}

/* x * 2^n with a single rounding: the scaling is split so intermediate
 * products are exact, and only the last multiplication can round. */
double scalbn(double x, int n)
{
	double x0 = x;
	if (n > 1023) {
		x *= 0x1p1023;
		n -= 1023;
		if (n > 1023) {
			x *= 0x1p1023;
			n -= 1023;
			if (n > 1023) n = 1023;
		}
	} else if (n < -1022) {
		x *= 0x1p-1022 * 0x1p53;
		n += 1022 - 53;
		if (n < -1022) {
			x *= 0x1p-1022 * 0x1p53;
			n += 1022 - 53;
			if (n < -1022) n = -1022;
		}
	}
	double y = x * pow2i(n);
	if (__builtin_isinf(y) && !__builtin_isinf(x0)) errno = ERANGE;
	else if (y == 0 && x0 != 0) errno = ERANGE;
	return y;
}

double scalbln(double x, long n)
{
	if (n > INT_MAX) n = INT_MAX;
	else if (n < INT_MIN) n = INT_MIN;
	return scalbn(x, (int)n);
}

double ldexp(double x, int n)
{
	return scalbn(x, n);
}

int ilogb(double x)
{
	uint64_t u = asuint64(x);
	int e = (int)(u >> 52 & 0x7ff);
	if (!e) {
		u <<= 12;
		if (!u) {
			fp_force_eval(fp_barrier(0.0) / 0.0);
			errno = EDOM;
			return FP_ILOGB0;
		}
		return -1023 - (__builtin_clzll(u));
	}
	if (e == 0x7ff) {
		fp_force_eval(fp_barrier(0.0) / 0.0);
		errno = EDOM;
		return (u << 12) ? FP_ILOGBNAN : INT_MAX;
	}
	return e - 0x3ff;
}

double logb(double x)
{
	if (!__builtin_isfinite(x)) return x * x;
	if (x == 0) return fp_barrier(-1.0) / 0.0;   /* pole: divbyzero only */
	return (double)ilogb(x);
}

double modf(double x, double *ip)
{
	uint64_t u = asuint64(x);
	int e = (int)(u >> 52 & 0x7ff) - 0x3ff;
	if (e >= 52) {
		*ip = x;
		if (e == 0x400 && (u << 12)) return x + x;    /* NaN */
		return asdouble(u & (1ULL << 63));
	}
	if (e < 0) {
		*ip = asdouble(u & (1ULL << 63));
		return x;
	}
	uint64_t m = 0x000fffffffffffffULL >> e;
	if (!(u & m)) {
		*ip = x;
		return asdouble(u & (1ULL << 63));
	}
	*ip = asdouble(u & ~m);
	return x - *ip;
}

double nextafter(double x, double y)
{
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	uint64_t ux = asuint64(x), uy = asuint64(y);
	if (ux == uy) return y;
	uint64_t ax = ux & ~(1ULL << 63), ay = uy & ~(1ULL << 63);
	if (!ax) {
		if (!ay) return y;
		ux = (uy & (1ULL << 63)) | 1;
	} else if (ax > ay || ((ux ^ uy) >> 63)) {
		ux--;
	} else {
		ux++;
	}
	double r = asdouble(ux);
	uint64_t e = ux >> 52 & 0x7ff;
	if (e == 0x7ff) {
		fp_force_eval(x + x);           /* overflow + inexact */
		errno = ERANGE;
	} else if (!e) {
		fp_force_eval(x * x + r * r);   /* underflow + inexact */
		if (ax) errno = ERANGE;         /* stepping off zero is no error */
	}
	return r;
}

double nexttoward(double x, long double y)
{
	if (__builtin_isnan(x) || __builtin_isnan(y)) return (double)((long double)x + y);
	if ((long double)x == y) return (double)y;
	double t = (long double)x < y ? __builtin_inf() : -__builtin_inf();
	return nextafter(x, t);
}

/* nan("n-char-sequence"): the sequence, read as an unsigned integer in
 * base 10, 16 (0x prefix) or 8 (0 prefix), becomes the NaN payload. */
static uint64_t nan_payload(const char *s)
{
	uint64_t v = 0;
	int base = 10;
	if (!s) return 0;
	if (s[0] == '0' && (s[1] | 32) == 'x') { base = 16; s += 2; }
	else if (s[0] == '0') base = 8;
	for (; *s; s++) {
		unsigned c = (unsigned char)*s, d;
		if (c - '0' < 10u) d = c - '0';
		else if ((c | 32) - 'a' < 6u) d = (c | 32) - 'a' + 10;
		else return 0;
		if (d >= (unsigned)base) return 0;
		v = v * (unsigned)base + d;
	}
	return v;
}

double nan(const char *s)
{
	return asdouble(0x7ff8000000000000ULL | (nan_payload(s) & 0x0007ffffffffffffULL));
}

double fdim(double x, double y)
{
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	return x > y ? x - y : 0.0;
}

double fmax(double x, double y)
{
	if (__builtin_isnan(x)) return y;
	if (__builtin_isnan(y)) return x;
	if (__builtin_signbit(x) != __builtin_signbit(y)) return __builtin_signbit(x) ? y : x;
	return x < y ? y : x;
}

double fmin(double x, double y)
{
	if (__builtin_isnan(x)) return y;
	if (__builtin_isnan(y)) return x;
	if (__builtin_signbit(x) != __builtin_signbit(y)) return __builtin_signbit(x) ? x : y;
	return x < y ? x : y;
}

double significand(double x)
{
	if (!__builtin_isfinite(x) || x == 0) return x;
	return scalbn(x, -ilogb(x));
}

double scalb(double x, double fn)
{
	if (__builtin_isnan(x) || __builtin_isnan(fn)) return x * fn;
	if (!__builtin_isfinite(fn)) {
		if (fn > 0) return x * fn;
		return x / -fn;
	}
	if (rint(fn) != fn) return __math_invalid(fn);
	if (fn > 65000.0) return scalbn(x, 65000);
	if (-fn > 65000.0) return scalbn(x, -65000);
	return scalbn(x, (int)fn);
}

int finite(double x) { return __builtin_isfinite(x); }
int __finite(double x) { return __builtin_isfinite(x); }
int __isinf(double x) { return __builtin_isinf_sign(x); }
int __isnan(double x) { return __builtin_isnan(x); }
int __signbit(double x) { return (int)(asuint64(x) >> 63); }
int __fpclassify(double x) { return fpclassify(x); }

/* ---------------------------------------------------------------- float */

float (copysignf)(float x, float y)
{
	return asfloat((asuint(x) & 0x7fffffffu) | (asuint(y) & 0x80000000u));
}

float (fabsf)(float x)
{
	return asfloat(asuint(x) & 0x7fffffffu);
}

float frexpf(float x, int *e)
{
	uint32_t u = asuint(x);
	int ee = (int)(u >> 23 & 0xff);
	if (!ee) {
		if (x != 0) {
			x = frexpf(x * 0x1p64f, e);
			*e -= 64;
		} else {
			*e = 0;
		}
		return x;
	}
	if (ee == 0xff) {
		*e = 0;
		return x + x;
	}
	*e = ee - 0x7e;
	return asfloat((u & 0x807fffffu) | 0x3f000000u);
}

float scalbnf(float x, int n)
{
	/* double has room for any float times 2^n with a single rounding */
	if (n > 300) n = 300;
	else if (n < -350) n = -350;
	return __math_narrowf((double)x * pow2i(n));
}

float scalblnf(float x, long n)
{
	if (n > INT_MAX) n = INT_MAX;
	else if (n < INT_MIN) n = INT_MIN;
	return scalbnf(x, (int)n);
}

float ldexpf(float x, int n)
{
	return scalbnf(x, n);
}

int ilogbf(float x)
{
	uint32_t u = asuint(x);
	int e = (int)(u >> 23 & 0xff);
	if (!e) {
		u <<= 9;
		if (!u) {
			fp_force_evalf(fp_barrierf(0.0f) / 0.0f);
			errno = EDOM;
			return FP_ILOGB0;
		}
		return -127 - __builtin_clz(u);
	}
	if (e == 0xff) {
		fp_force_evalf(fp_barrierf(0.0f) / 0.0f);
		errno = EDOM;
		return (u << 9) ? FP_ILOGBNAN : INT_MAX;
	}
	return e - 0x7f;
}

float logbf(float x)
{
	if (!__builtin_isfinite(x)) return x * x;
	if (x == 0) return fp_barrierf(-1.0f) / 0.0f;
	return (float)ilogbf(x);
}

float modff(float x, float *ip)
{
	uint32_t u = asuint(x);
	int e = (int)(u >> 23 & 0xff) - 0x7f;
	if (e >= 23) {
		*ip = x;
		if (e == 0x80 && (u << 9)) return x + x;
		return asfloat(u & 0x80000000u);
	}
	if (e < 0) {
		*ip = asfloat(u & 0x80000000u);
		return x;
	}
	uint32_t m = 0x007fffffu >> e;
	if (!(u & m)) {
		*ip = x;
		return asfloat(u & 0x80000000u);
	}
	*ip = asfloat(u & ~m);
	return x - *ip;
}

float nextafterf(float x, float y)
{
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	uint32_t ux = asuint(x), uy = asuint(y);
	if (ux == uy) return y;
	uint32_t ax = ux & 0x7fffffffu, ay = uy & 0x7fffffffu;
	if (!ax) {
		if (!ay) return y;
		ux = (uy & 0x80000000u) | 1;
	} else if (ax > ay || ((ux ^ uy) >> 31)) {
		ux--;
	} else {
		ux++;
	}
	float r = asfloat(ux);
	uint32_t e = ux >> 23 & 0xff;
	if (e == 0xff) {
		fp_force_evalf(x + x);
		errno = ERANGE;
	} else if (!e) {
		fp_force_evalf(x * x + r * r);
		if (ax) errno = ERANGE;
	}
	return r;
}

float nexttowardf(float x, long double y)
{
	if (__builtin_isnan(x) || __builtin_isnan(y)) return (float)((long double)x + y);
	if ((long double)x == y) return (float)y;
	return nextafterf(x, (long double)x < y ? __builtin_inff() : -__builtin_inff());
}

float nanf(const char *s)
{
	return asfloat(0x7fc00000u | (uint32_t)(nan_payload(s) & 0x003fffffu));
}

float fdimf(float x, float y)
{
	if (__builtin_isnan(x) || __builtin_isnan(y)) return x + y;
	return x > y ? x - y : 0.0f;
}

float fmaxf(float x, float y)
{
	if (__builtin_isnan(x)) return y;
	if (__builtin_isnan(y)) return x;
	if (__builtin_signbit(x) != __builtin_signbit(y)) return __builtin_signbit(x) ? y : x;
	return x < y ? y : x;
}

float fminf(float x, float y)
{
	if (__builtin_isnan(x)) return y;
	if (__builtin_isnan(y)) return x;
	if (__builtin_signbit(x) != __builtin_signbit(y)) return __builtin_signbit(x) ? x : y;
	return x < y ? x : y;
}

float significandf(float x)
{
	if (!__builtin_isfinite(x) || x == 0) return x;
	return scalbnf(x, -ilogbf(x));
}

float scalbf(float x, float fn)
{
	return __math_narrowf(scalb(x, fn));
}

int finitef(float x) { return __builtin_isfinite(x); }
int __finitef(float x) { return __builtin_isfinite(x); }
int __isinff(float x) { return __builtin_isinf_sign(x); }
int __isnanf(float x) { return __builtin_isnan(x); }
int __signbitf(float x) { return (int)(asuint(x) >> 31); }
int __fpclassifyf(float x) { return fpclassify(x); }
