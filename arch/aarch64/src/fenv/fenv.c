/*
 * lib-spfxd — floating-point environment (AArch64).
 *
 * FPSR holds the cumulative exception flags (bits 0-4, the FE_* values),
 * FPCR the rounding mode (bits 22-23) and the trap enables (bits 8-12,
 * the FE_* values shifted by 8).  Many implementations do not support
 * trapping; there the enable bits read as zero and feenableexcept reports
 * failure.
 */
#include <fenv.h>

#define TRAP_SHIFT 8

static __inline unsigned long get_fpsr(void)
{
	unsigned long v;
	__asm__ __volatile__ ("mrs %0, fpsr" : "=r"(v));
	return v;
}

static __inline void set_fpsr(unsigned long v)
{
	__asm__ __volatile__ ("msr fpsr, %0" : : "r"(v) : "memory");
}

static __inline unsigned long get_fpcr(void)
{
	unsigned long v;
	__asm__ __volatile__ ("mrs %0, fpcr" : "=r"(v));
	return v;
}

static __inline void set_fpcr(unsigned long v)
{
	__asm__ __volatile__ ("msr fpcr, %0" : : "r"(v) : "memory");
}

int feclearexcept(int e)
{
	set_fpsr(get_fpsr() & ~(unsigned long)(e & FE_ALL_EXCEPT));
	return 0;
}

int feraiseexcept(int e)
{
	set_fpsr(get_fpsr() | (unsigned long)(e & FE_ALL_EXCEPT));
	return 0;
}

int fetestexcept(int e)
{
	return (int)(get_fpsr() & (unsigned long)(e & FE_ALL_EXCEPT));
}

int fegetexceptflag(fexcept_t *f, int e)
{
	*f = (fexcept_t)(get_fpsr() & (unsigned long)(e & FE_ALL_EXCEPT));
	return 0;
}

int fesetexceptflag(const fexcept_t *f, int e)
{
	e &= FE_ALL_EXCEPT;
	set_fpsr((get_fpsr() & ~(unsigned long)e) | (*f & (unsigned long)e));
	return 0;
}

int fegetround(void)
{
	return (int)(get_fpcr() & 0xc00000);
}

int fesetround(int r)
{
	if (r & ~0xc00000) return -1;
	set_fpcr((get_fpcr() & ~0xc00000UL) | (unsigned long)r);
	return 0;
}

int fegetenv(fenv_t *env)
{
	env->__fpcr = (unsigned int)get_fpcr();
	env->__fpsr = (unsigned int)get_fpsr();
	return 0;
}

int fesetenv(const fenv_t *env)
{
	if (env == FE_DFL_ENV) {
		set_fpcr(0);
		set_fpsr(0);
	} else {
		set_fpcr(env->__fpcr);
		set_fpsr(env->__fpsr);
	}
	return 0;
}

int feholdexcept(fenv_t *env)
{
	fegetenv(env);
	set_fpsr(0);
	set_fpcr(get_fpcr() & ~((unsigned long)FE_ALL_EXCEPT << TRAP_SHIFT));
	return 0;
}

int feupdateenv(const fenv_t *env)
{
	int e = fetestexcept(FE_ALL_EXCEPT);
	fesetenv(env);
	feraiseexcept(e);
	return 0;
}

/* GNU extensions: trap enables.  Return the previous set, or -1 if the
 * hardware did not accept the request. */
int feenableexcept(int e)
{
	unsigned long old = get_fpcr();
	e &= FE_ALL_EXCEPT;
	set_fpcr(old | ((unsigned long)e << TRAP_SHIFT));
	if ((get_fpcr() >> TRAP_SHIFT & FE_ALL_EXCEPT) != ((old >> TRAP_SHIFT | (unsigned long)e) & FE_ALL_EXCEPT)) {
		set_fpcr(old);
		return -1;
	}
	return (int)(old >> TRAP_SHIFT & FE_ALL_EXCEPT);
}

int fedisableexcept(int e)
{
	unsigned long old = get_fpcr();
	set_fpcr(old & ~((unsigned long)(e & FE_ALL_EXCEPT) << TRAP_SHIFT));
	return (int)(old >> TRAP_SHIFT & FE_ALL_EXCEPT);
}

int fegetexcept(void)
{
	return (int)(get_fpcr() >> TRAP_SHIFT & FE_ALL_EXCEPT);
}

/* FLT_ROUNDS: 0 toward zero, 1 nearest, 2 upward, 3 downward */
int __spfxd_flt_rounds(void)
{
	switch (fegetround()) {
	case FE_TOWARDZERO: return 0;
	case FE_UPWARD: return 2;
	case FE_DOWNWARD: return 3;
	default: return 1;
	}
}
