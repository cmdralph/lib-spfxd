/*
 * lib-spfxd — floating-point environment (x86-64).
 *
 * Two units hold floating-point state: the x87 FPU (long double) and SSE
 * (float/double).  Every operation here keeps them consistent: rounding
 * modes and exception masks are set in both, and the exception flags
 * reported are the union of both units' sticky flags.
 *
 *   x87 control word:  bits 0-5 exception masks, bits 10-11 rounding
 *   x87 status word:   bits 0-5 exception flags
 *   MXCSR:             bits 0-5 flags, bits 7-12 masks, bits 13-14 rounding
 */
#include <fenv.h>
#include <float.h>
#include "libc.h"

static __inline unsigned get_mxcsr(void)
{
	unsigned v;
	__asm__ __volatile__ ("stmxcsr %0" : "=m"(v));
	return v;
}

static __inline void set_mxcsr(unsigned v)
{
	__asm__ __volatile__ ("ldmxcsr %0" : : "m"(v));
}

static __inline unsigned short get_cw(void)
{
	unsigned short v;
	__asm__ __volatile__ ("fnstcw %0" : "=m"(v));
	return v;
}

static __inline void set_cw(unsigned short v)
{
	__asm__ __volatile__ ("fldcw %0" : : "m"(v));
}

static __inline unsigned short get_sw(void)
{
	unsigned short v;
	__asm__ __volatile__ ("fnstsw %0" : "=m"(v));
	return v;
}

int feclearexcept(int e)
{
	e &= FE_ALL_EXCEPT;
	if (!e) return 0;
	/* x87: the status word can only be written through the environment */
	fenv_t env;
	__asm__ __volatile__ ("fnstenv %0" : "=m"(env));
	env.__status_word &= (unsigned short)~e;
	__asm__ __volatile__ ("fldenv %0" : : "m"(env));
	set_mxcsr(get_mxcsr() & ~(unsigned)e);
	return 0;
}

int feraiseexcept(int e)
{
	/* Raise through real arithmetic so unmasked exceptions trap, as an
	 * operation producing them would. */
	volatile double zero = 0.0, one = 1.0, big = DBL_MAX, tiny = DBL_MIN, r;
	if (e & FE_INVALID) r = zero / zero;
	if (e & FE_DIVBYZERO) r = one / zero;
	if (e & FE_OVERFLOW) r = big * big;
	if (e & FE_UNDERFLOW) r = tiny * tiny;
	if (e & FE_INEXACT) r = one + tiny;
	(void)r;
	return 0;
}

int fetestexcept(int e)
{
	return (int)((get_sw() | get_mxcsr()) & (unsigned)e & FE_ALL_EXCEPT);
}

int fegetexceptflag(fexcept_t *f, int e)
{
	*f = (fexcept_t)fetestexcept(e);
	return 0;
}

int fesetexceptflag(const fexcept_t *f, int e)
{
	e &= FE_ALL_EXCEPT;
	feclearexcept(e);
	/* set flags without trapping */
	fenv_t env;
	__asm__ __volatile__ ("fnstenv %0" : "=m"(env));
	env.__status_word |= (unsigned short)(*f & e);
	__asm__ __volatile__ ("fldenv %0" : : "m"(env));
	return 0;
}

int fegetround(void)
{
	return get_cw() & 0xc00;
}

int fesetround(int r)
{
	if ((unsigned)r & ~0xc00u) return -1;
	set_cw((unsigned short)((get_cw() & ~0xc00) | r));
	set_mxcsr((get_mxcsr() & ~0x6000u) | (unsigned)r << 3);
	return 0;
}

int fegetenv(fenv_t *env)
{
	unsigned short cw = get_cw();
	__asm__ __volatile__ ("fnstenv %0" : "=m"(*env));
	set_cw(cw);   /* fnstenv masks all x87 exceptions as a side effect */
	env->__mxcsr = get_mxcsr();
	return 0;
}

int fesetenv(const fenv_t *env)
{
	fenv_t def;
	if (env == FE_DFL_ENV) {
		__asm__ __volatile__ ("fnstenv %0" : "=m"(def));
		def.__control_word = 0x37f;
		def.__status_word = 0;
		def.__tags = 0xffff;
		__asm__ __volatile__ ("fldenv %0" : : "m"(def));
		set_mxcsr(0x1f80);
		return 0;
	}
	__asm__ __volatile__ ("fldenv %0" : : "m"(*env));
	set_mxcsr(env->__mxcsr);
	return 0;
}

int feholdexcept(fenv_t *env)
{
	fegetenv(env);
	feclearexcept(FE_ALL_EXCEPT);
	set_cw(get_cw() | FE_ALL_EXCEPT);
	set_mxcsr(get_mxcsr() | FE_ALL_EXCEPT << 7);
	return 0;
}

int feupdateenv(const fenv_t *env)
{
	int raised = fetestexcept(FE_ALL_EXCEPT);
	fesetenv(env);
	feraiseexcept(raised);
	return 0;
}

int feenableexcept(int e)
{
	int old = ~get_cw() & FE_ALL_EXCEPT;
	e &= FE_ALL_EXCEPT;
	set_cw((unsigned short)(get_cw() & ~e));
	set_mxcsr(get_mxcsr() & ~((unsigned)e << 7));
	return old;
}

int fedisableexcept(int e)
{
	int old = ~get_cw() & FE_ALL_EXCEPT;
	e &= FE_ALL_EXCEPT;
	set_cw((unsigned short)(get_cw() | e));
	set_mxcsr(get_mxcsr() | (unsigned)e << 7);
	return old;
}

int fegetexcept(void)
{
	return ~get_cw() & FE_ALL_EXCEPT;
}

/* FLT_ROUNDS: 0 toward zero, 1 nearest, 2 upward, 3 downward */
int __spfxd_flt_rounds(void)
{
	switch (fegetround()) {
	case FE_TOWARDZERO: return 0;
	case FE_UPWARD: return 2;
	case FE_DOWNWARD: return 3;
	}
	return 1;
}
