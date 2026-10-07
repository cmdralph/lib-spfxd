/* lib-spfxd test — <fenv.h>: exception flags, rounding modes affecting
 * arithmetic, conversions, rint and printf, environments. */
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include "t.h"

static volatile double one = 1.0, three = 3.0, zero = 0.0, big = DBL_MAX, tiny = DBL_MIN;

int main(void)
{
	feclearexcept(FE_ALL_EXCEPT);
	CHECK(!fetestexcept(FE_ALL_EXCEPT), "clear");
	volatile double r = one / zero;
	CHECK(fetestexcept(FE_DIVBYZERO) && !fetestexcept(FE_INVALID), "divbyzero");
	r = zero / zero;
	CHECK(fetestexcept(FE_INVALID), "invalid");
	r = big * big;
	CHECK(fetestexcept(FE_OVERFLOW | FE_INEXACT) == (FE_OVERFLOW | FE_INEXACT), "overflow");
	feclearexcept(FE_ALL_EXCEPT);
	r = tiny * tiny;
	CHECK(fetestexcept(FE_UNDERFLOW), "underflow");
	feclearexcept(FE_ALL_EXCEPT);
	volatile long double lr = (long double)one / 0;
	CHECK(fetestexcept(FE_DIVBYZERO), "x87 flags reported");
	(void)lr;
	CHECK(!feraiseexcept(FE_INEXACT) && fetestexcept(FE_INEXACT), "feraiseexcept");
	fexcept_t fl;
	fegetexceptflag(&fl, FE_ALL_EXCEPT);
	feclearexcept(FE_ALL_EXCEPT);
	fesetexceptflag(&fl, FE_ALL_EXCEPT);
	CHECK(fetestexcept(FE_INEXACT), "save/restore flags");
	/* rounding modes */
	int modes[4] = { FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO };
	double expect_div[4] = { 0x1.5555555555555p-2, 0x1.5555555555556p-2, 0x1.5555555555555p-2, 0x1.5555555555555p-2 };
	double expect_rint[4] = { 2.0, 3.0, 2.0, 2.0 };
	for (int i = 0; i < 4; i++) {
		CHECK(!fesetround(modes[i]) && fegetround() == modes[i], "fesetround %d", i);
		volatile double q = one / three;
		CHECK(q == expect_div[i], "1/3 in mode %d: %a", i, q);
		CHECK(rint(2.5) == expect_rint[i], "rint(2.5) mode %d", i);
		CHECK(lrint(-2.5) == (i == 1 ? -2 : i == 2 ? -3 : -2), "lrint(-2.5) mode %d", i);
		volatile long double lq = (long double)one / three;
		CHECK(rintl(2.5L) == expect_rint[i], "x87 rounding follows (mode %d)", i);
		(void)lq;
		CHECK(nearbyint(-0.5) == (i == 2 ? -1.0 : -0.0), "nearbyint mode %d", i);
	}
	fesetround(FE_TONEAREST);
	CHECK(fesetround(12345) != 0, "invalid mode rejected");
	char b[32];
	fesetround(FE_UPWARD);
	snprintf(b, sizeof b, "%.1f", 0.125);
	CHECK(!strcmp(b, "0.2"), "printf honours rounding mode: %s", b);
	fesetround(FE_DOWNWARD);
	snprintf(b, sizeof b, "%.1f", 0.175);
	CHECK(!strcmp(b, "0.1"), "printf downward: %s", b);
	fesetround(FE_TONEAREST);
	CHECK(FLT_ROUNDS == 1, "FLT_ROUNDS");
	/* environments */
	fenv_t env;
	fesetround(FE_UPWARD);
	feraiseexcept(FE_OVERFLOW);
	CHECK(!feholdexcept(&env) && !fetestexcept(FE_ALL_EXCEPT) && fegetround() == FE_UPWARD, "feholdexcept");
	feraiseexcept(FE_INEXACT);
	CHECK(!feupdateenv(&env) && fetestexcept(FE_INEXACT | FE_OVERFLOW) == (FE_INEXACT | FE_OVERFLOW), "feupdateenv merges");
	CHECK(!fesetenv(FE_DFL_ENV) && fegetround() == FE_TONEAREST, "FE_DFL_ENV");
	(void)r;
	return DONE();
}
