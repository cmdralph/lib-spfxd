/* lib-spfxd — <stdarg.h>.  The x86-64 va_list is a compiler-defined structure,
 * so the only correct implementation is the compiler's builtins. */
#ifndef _STDARG_H
#define _STDARG_H

#define __SPFXD_NEED_va_list
#include <bits/typedefs.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ > 201710L
# define va_start(ap, ...) __builtin_va_start(ap, 0)
#else
# define va_start(ap, last) __builtin_va_start(ap, last)
#endif
#define va_end(ap)          __builtin_va_end(ap)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_copy(dst, src)   __builtin_va_copy(dst, src)
#define __va_copy(dst, src) __builtin_va_copy(dst, src)

#endif
