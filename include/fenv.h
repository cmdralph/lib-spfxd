/* lib-spfxd — <fenv.h> */
#ifndef _FENV_H
#define _FENV_H
#include <features.h>
#include <bits/fenv.h>

__SPFXD_BEGIN_DECLS
int feclearexcept(int);
int fegetexceptflag(fexcept_t *, int);
int feraiseexcept(int);
int fesetexceptflag(const fexcept_t *, int);
int fetestexcept(int);
int fegetround(void);
int fesetround(int);
int fegetenv(fenv_t *);
int feholdexcept(fenv_t *);
int fesetenv(const fenv_t *);
int feupdateenv(const fenv_t *);
#if defined(__SPFXD_GNU)
int feenableexcept(int);
int fedisableexcept(int);
int fegetexcept(void);
#endif
__SPFXD_END_DECLS
#endif
