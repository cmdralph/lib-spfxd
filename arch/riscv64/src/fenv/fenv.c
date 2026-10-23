/*
 * lib-spfxd — <fenv.h> for RISC-V: fcsr holds the accrued exception
 * flags (fflags, bits 0-4) and the dynamic rounding mode (frm, bits 5-7).
 * RISC-V floating point never traps, so the GNU trap-enable extensions
 * report that no exception can be enabled.
 */
#include <fenv.h>
#include <float.h>

static __inline unsigned get_fcsr(void)
{
	unsigned v;
	__asm__ __volatile__ ("frcsr %0" : "=r"(v));
	return v;
}

static __inline void set_fcsr(unsigned v)
{
	__asm__ __volatile__ ("fscsr %0" : : "r"(v) : "memory");
}

static __inline unsigned get_flags(void)
{
	unsigned v;
	__asm__ __volatile__ ("frflags %0" : "=r"(v));
	return v;
}

int feclearexcept(int e)
{
	__asm__ __volatile__ ("csrc fflags, %0" : : "r"(e & FE_ALL_EXCEPT) : "memory");
	return 0;
}

int feraiseexcept(int e)
{
	__asm__ __volatile__ ("csrs fflags, %0" : : "r"(e & FE_ALL_EXCEPT) : "memory");
	return 0;
}

int fetestexcept(int e)
{
	return (int)(get_flags() & (unsigned)(e & FE_ALL_EXCEPT));
}

int fegetexceptflag(fexcept_t *f, int e)
{
	*f = get_flags() & (unsigned)(e & FE_ALL_EXCEPT);
	return 0;
}

int fesetexceptflag(const fexcept_t *f, int e)
{
	e &= FE_ALL_EXCEPT;
	feclearexcept(e);
	feraiseexcept((int)(*f & (unsigned)e));
	return 0;
}

int fegetround(void)
{
	unsigned r;
	__asm__ __volatile__ ("frrm %0" : "=r"(r));
	return (int)r;
}

int fesetround(int r)
{
	if ((unsigned)r > FE_UPWARD) return -1;
	__asm__ __volatile__ ("fsrm %0" : : "r"(r) : "memory");
	return 0;
}

int fegetenv(fenv_t *env)
{
	*env = get_fcsr();
	return 0;
}

int fesetenv(const fenv_t *env)
{
	set_fcsr(env == FE_DFL_ENV ? 0 : *env);
	return 0;
}

int feholdexcept(fenv_t *env)
{
	fegetenv(env);
	feclearexcept(FE_ALL_EXCEPT);
	return 0;
}

int feupdateenv(const fenv_t *env)
{
	int e = fetestexcept(FE_ALL_EXCEPT);
	fesetenv(env);
	feraiseexcept(e);
	return 0;
}

/* GNU extensions: no trapping exists, so nothing can be enabled. */
int feenableexcept(int e)
{
	return (e & FE_ALL_EXCEPT) ? -1 : 0;
}

int fedisableexcept(int e)
{
	return 0;
}

int fegetexcept(void)
{
	return 0;
}

/* FLT_ROUNDS: 0 toward zero, 1 nearest, 2 upward, 3 downward; 4 for the
 * RISC-V mode "nearest, ties away from zero" (RMM) */
int __spfxd_flt_rounds(void)
{
	switch (fegetround()) {
	case FE_TOWARDZERO: return 0;
	case FE_UPWARD: return 2;
	case FE_DOWNWARD: return 3;
	case FE_TONEAREST: return 1;
	default: return 4;
	}
}
