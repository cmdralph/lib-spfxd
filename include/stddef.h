/* lib-spfxd — <stddef.h> */
#ifndef _STDDEF_H
#define _STDDEF_H

#ifdef __cplusplus
# define NULL 0L
#else
# define NULL ((void *)0)
#endif

#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_ptrdiff_t
#define __SPFXD_NEED_wchar_t
#define __SPFXD_NEED_max_align_t
#include <bits/typedefs.h>

#define offsetof(type, member) __builtin_offsetof(type, member)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ > 201710L
# define unreachable() __builtin_unreachable()
#endif

#endif
