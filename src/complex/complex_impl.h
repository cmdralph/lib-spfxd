/*
 * lib-spfxd — complex elementary functions, written once for a real type.
 *
 * Included by cdouble.c (T = double) and cldouble.c (T = long double)
 * with these macros defined:
 *   T            the real type             CT     T _Complex
 *   F(name)      the real function name    CF(name)  the complex name
 *   C(x)         a T literal
 *   T_MAX, T_EPS, T_MANT
 *
 * The inverse functions use Kahan's formulas ("Branch Cuts for Complex
 * Elementary Functions", 1987): with the square roots of 1 - z and 1 + z
 * computed separately, signed zeros select the correct side of every
 * branch cut and no cancellation occurs near +-1.  Infinities and NaNs
 * follow C11 Annex G for the cases listed in each function.
 */

static always_inline CT mk(T re, T im)
{
	CT z;
	__real__ z = re;
	__imag__ z = im;
	return z;
}

T CF(creal)(CT z) { return __real__ z; }
T CF(cimag)(CT z) { return __imag__ z; }
CT CF(conj)(CT z) { return mk(__real__ z, -__imag__ z); }
T CF(cabs)(CT z) { return F(hypot)(__real__ z, __imag__ z); }
T CF(carg)(CT z) { return F(atan2)(__imag__ z, __real__ z); }

CT CF(cproj)(CT z)
{
	if (__builtin_isinf(__real__ z) || __builtin_isinf(__imag__ z))
		return mk(__builtin_inf(), F(copysign)(C(0.0), __imag__ z));
	return z;
}

CT CF(cexp)(CT z)
{
	T x = __real__ z, y = __imag__ z;
	if (y == 0 && !__builtin_isnan(x)) return mk(F(exp)(x), y);
	if (__builtin_isinf(x)) {
		if (x < 0) {
			if (!__builtin_isfinite(y)) return mk(C(0.0), C(0.0));
			return mk(F(copysign)(C(0.0), F(cos)(y)), F(copysign)(C(0.0), F(sin)(y)));
		}
		if (!__builtin_isfinite(y)) return mk(x, y - y);     /* (+inf, NaN), invalid */
	}
	if (__builtin_isnan(x)) return mk(x, y == 0 ? y : x);
	T s, c;
	F(sincos)(y, &s, &c);
	if (x > C(700.0)) {
		/* exp(x) may overflow while exp(x) cos(y) does not */
		T e = F(exp)(x * C(0.5));
		return mk(c * e * e, s * e * e);
	}
	T e = F(exp)(x);
	return mk(e * c, e * s);
}

CT CF(clog)(CT z)
{
	T x = __real__ z, y = __imag__ z;
	T ax = F(fabs)(x), ay = F(fabs)(y), re;
	if (__builtin_isinf(x) || __builtin_isinf(y)) re = __builtin_inf();
	else if (__builtin_isnan(x) || __builtin_isnan(y)) re = x + y;
	else if (x == 0 && y == 0) re = -C(1.0) / ax;        /* -inf, divide-by-zero */
	else {
		if (ax < ay) { T t = ax; ax = ay; ay = t; }
		if (ax > C(0.5) && ax < C(2.0) && ay < C(1.0)) {
			/* near the unit circle: log|z| = log1p(x^2 + y^2 - 1)/2 with the
			 * argument formed exactly: (ax - 1)(ax + 1) + ay^2 */
			T a = (ax - C(1.0)) * (ax + C(1.0));
			re = C(0.5) * F(log1p)(a + ay * ay);
		} else {
			re = F(log)(F(hypot)(ax, ay));
		}
	}
	return mk(re, F(atan2)(y, x));
}

CT CF(csqrt)(CT z)
{
	T x = __real__ z, y = __imag__ z;
	if (__builtin_isinf(y)) return mk(__builtin_inf(), y);
	if (__builtin_isnan(x)) return mk(x, y - y + x);
	if (__builtin_isinf(x)) {
		if (__builtin_isnan(y)) return x > 0 ? mk(x, y) : mk(y, __builtin_inf());
		return x > 0 ? mk(x, F(copysign)(C(0.0), y)) : mk(C(0.0), F(copysign)(-x, y));
	}
	if (__builtin_isnan(y)) return mk(y, y);
	if (x == 0 && y == 0) return mk(C(0.0), y);
	/* scale away overflow and underflow */
	int sc = 0;
	T ax = F(fabs)(x), ay = F(fabs)(y);
	if (ax > T_MAX / 4 || ay > T_MAX / 4) { x *= C(0.25); y *= C(0.25); sc = 1; }
	else if (ax < T_MIN_SAFE && ay < T_MIN_SAFE) { x *= T_SCALE_UP; y *= T_SCALE_UP; sc = -1; }
	T t = F(sqrt)((F(fabs)(x) + F(hypot)(x, y)) * C(0.5));
	T re, im;
	if (x >= 0) { re = t; im = y / (2 * t); }
	else { re = F(fabs)(y) / (2 * t); im = F(copysign)(t, y); }
	if (sc > 0) { re *= 2; im *= 2; }
	else if (sc < 0) { re *= T_SCALE_DOWN_SQRT; im *= T_SCALE_DOWN_SQRT; }
	return mk(re, im);
}

CT CF(csinh)(CT z)
{
	T x = __real__ z, y = __imag__ z;
	if (y == 0 && !__builtin_isnan(x)) return mk(F(sinh)(x), y);
	if (x == 0 && !__builtin_isfinite(y)) return mk(x, y - y);
	if (__builtin_isinf(x) && !__builtin_isfinite(y)) return mk(x, y - y);
	T s, c;
	F(sincos)(y, &s, &c);
	if (__builtin_isinf(x)) return mk(F(copysign)(x, c), F(copysign)(x, s));
	return mk(F(sinh)(x) * c, F(cosh)(x) * s);
}

CT CF(ccosh)(CT z)
{
	T x = __real__ z, y = __imag__ z;
	if (y == 0 && !__builtin_isnan(x)) return mk(F(cosh)(x), x * y);
	if (x == 0 && !__builtin_isfinite(y)) return mk(y - y, C(0.0));
	if (__builtin_isinf(x) && !__builtin_isfinite(y)) return mk(F(fabs)(x), y - y);
	T s, c;
	F(sincos)(y, &s, &c);
	if (__builtin_isinf(x)) return mk(F(copysign)(F(fabs)(x), c), F(copysign)(F(fabs)(x), s) * (x < 0 ? -1 : 1));
	return mk(F(cosh)(x) * c, F(sinh)(x) * s);
}

CT CF(ctanh)(CT z)
{
	T x = __real__ z, y = __imag__ z;
	if (__builtin_isinf(x)) {
		T im = __builtin_isfinite(y) ? F(copysign)(C(0.0), F(sin)(2 * y)) : F(copysign)(C(0.0), y);
		return mk(F(copysign)(C(1.0), x), im);
	}
	if (__builtin_isnan(x)) return mk(x, y == 0 ? y : x);
	if (!__builtin_isfinite(y)) return mk(y - y, y - y);
	if (y == 0) return mk(F(tanh)(x), y);
	if (F(fabs)(x) > T_TANH_BIG) {
		/* tanh(x) = +-1; Im = 4 sin y cos y e^(-2|x|) */
		T e = F(exp)(-2 * F(fabs)(x));
		return mk(F(copysign)(C(1.0), x), 4 * F(sin)(y) * F(cos)(y) * e);
	}
	/* Kahan: t = tan y, b = 1 + t^2, s = sinh x, r = sqrt(1 + s^2) */
	T t = F(tan)(y), b = 1 + t * t, s = F(sinh)(x), r = F(sqrt)(1 + s * s);
	T d = 1 + b * s * s;
	return mk(b * r * s / d, t / d);
}

CT CF(csin)(CT z)
{
	CT w = CF(csinh)(mk(-__imag__ z, __real__ z));
	return mk(__imag__ w, -__real__ w);
}

CT CF(ccos)(CT z)
{
	return CF(ccosh)(mk(-__imag__ z, __real__ z));
}

CT CF(ctan)(CT z)
{
	CT w = CF(ctanh)(mk(-__imag__ z, __real__ z));
	return mk(__imag__ w, -__real__ w);
}

/* Large arguments: asin/acos/asinh need only log(2|z|) and arg. */
static int big(T x, T y)
{
	return F(fabs)(x) > T_BIG || F(fabs)(y) > T_BIG;
}

CT CF(casinh)(CT z);

CT CF(cacos)(CT z)
{
	T x = __real__ z, y = __imag__ z;
	if (__builtin_isnan(x) || __builtin_isnan(y)) {
		if (__builtin_isinf(x)) return mk(y, -x);
		if (__builtin_isinf(y)) return mk(x, -y);
		if (x == 0) return mk(F(atan2)(C(1.0), C(0.0)), y + y);
		return mk(x + y, x + y);
	}
	if (big(x, y) || __builtin_isinf(x) || __builtin_isinf(y)) {
		/* acos z = -i log(z + i sqrt(1 - z^2)) ~ arg(z) - i log(2|z|) */
		CT l = CF(clog)(z);
		T im = __real__ l + T_LN2;
		return mk(F(atan2)(F(fabs)(y), x), y >= 0 && !__builtin_signbit(y) ? -im : im);
	}
	CT s1 = CF(csqrt)(mk(1 - x, -y)), s2 = CF(csqrt)(mk(1 + x, y));
	T re = 2 * F(atan2)(__real__ s1, __real__ s2);
	T im = F(asinh)(__real__ s2 * __imag__ s1 - __imag__ s2 * __real__ s1);
	return mk(re, im);
}

CT CF(casin)(CT z)
{
	T x = __real__ z, y = __imag__ z;
	/* asin z = -i asinh(i z) */
	CT w = CF(casinh)(mk(-y, x));
	return mk(__imag__ w, -__real__ w);
}

CT CF(casinh)(CT z)
{
	T x = __real__ z, y = __imag__ z;
	if (__builtin_isnan(x) || __builtin_isnan(y)) {
		if (__builtin_isinf(x)) return mk(x, y + y);
		if (__builtin_isinf(y)) return mk(y, x + x);
		if (y == 0) return mk(x + x, y);
		return mk(x + y, x + y);
	}
	if (big(x, y) || __builtin_isinf(x) || __builtin_isinf(y)) {
		/* asinh z ~ log(2z) for |z| large, in the half plane of x */
		CT l = CF(clog)(mk(F(fabs)(x), y));
		return mk(F(copysign)(__real__ l + T_LN2, x), __imag__ l);
	}
	/* Kahan: asinh z = -i asin(i z) with
	 *   asin w = atan(Re w / Re(sqrt(1-w) sqrt(1+w))) + i asinh(Im(conj(sqrt(1-w)) sqrt(1+w))) */
	T wr = -y, wi = x;                       /* w = i z */
	CT s1 = CF(csqrt)(mk(1 - wr, -wi)), s2 = CF(csqrt)(mk(1 + wr, wi));
	T den = __real__ s1 * __real__ s2 - __imag__ s1 * __imag__ s2;
	T are = F(atan2)(wr, den);
	T aim = F(asinh)(__real__ s1 * __imag__ s2 - __imag__ s1 * __real__ s2);
	/* asinh z = -i (are + i aim) = aim - i are */
	return mk(aim, -are);
}

CT CF(cacosh)(CT z)
{
	T x = __real__ z, y = __imag__ z;
	/* acosh z = +-i acos z, choosing the sign that makes Re >= 0 */
	CT w = CF(cacos)(z);
	T re = __imag__ w, im = __real__ w;
	if (__builtin_isnan(re) && __builtin_isnan(im)) return mk(re, im);
	(void)x;
	return mk(F(fabs)(re), F(copysign)(im, y));
}

CT CF(catanh)(CT z)
{
	T x = __real__ z, y = __imag__ z;
	T ax = F(fabs)(x), ay = F(fabs)(y);
	if (__builtin_isnan(x) || __builtin_isnan(y)) {
		if (__builtin_isinf(y)) return mk(F(copysign)(C(0.0), x), F(copysign)(T_PIO2, y));
		if (__builtin_isinf(x) || x == 0) return mk(F(copysign)(C(0.0), x), x + y);
		return mk(x + y, x + y);
	}
	if (ax > T_BIG || ay > T_BIG) {
		/* Re = x / |z|^2 (tiny), Im = +-pi/2 */
		T re;
		if (ax >= ay) re = C(1.0) / (x + (y / x) * y);
		else re = (x / y) / y / (1 + (x / y) * (x / y));
		return mk(re, F(copysign)(T_PIO2, y));
	}
	if (ax == 1 && y == 0) return mk(x / C(0.0), y);  /* pole */
	/* Re = log1p(4x / ((1-x)^2 + y^2)) / 4, Im = atan2(2y, (1-x)(1+x) - y^2) / 2 */
	T omx = 1 - ax;
	T re = C(0.25) * F(log1p)(4 * ax / (omx * omx + ay * ay));
	T im = C(0.5) * F(atan2)(2 * y, omx * (1 + ax) - ay * ay);
	return mk(F(copysign)(re, x), im);
}

CT CF(catan)(CT z)
{
	/* atan z = -i atanh(i z) */
	CT w = CF(catanh)(mk(-__imag__ z, __real__ z));
	return mk(__imag__ w, -__real__ w);
}

CT CF(cpow)(CT z, CT w)
{
	T x = __real__ z, y = __imag__ z;
	if (x == 0 && y == 0) {
		if (__imag__ w == 0 && __real__ w > 0) return mk(C(0.0), C(0.0));
	}
	/* real base and real exponent: use the real pow for exactness */
	if (y == 0 && __imag__ w == 0 && x > 0 && !__builtin_signbit(y))
		return mk(F(pow)(x, __real__ w), y);
	CT l = CF(clog)(z);
	CT p = mk(__real__ w * __real__ l - __imag__ w * __imag__ l,
	          __real__ w * __imag__ l + __imag__ w * __real__ l);
	return CF(cexp)(p);
}

CT CF(clog10)(CT z)
{
	CT l = CF(clog)(z);
	return mk(__real__ l * T_LOG10E, __imag__ l * T_LOG10E);
}
