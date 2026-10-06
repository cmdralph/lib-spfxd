/* lib-spfxd — <monetary.h> */
#ifndef _MONETARY_H
#define _MONETARY_H
#include <features.h>
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_ssize_t
#define __SPFXD_NEED_locale_t
#include <bits/typedefs.h>
__SPFXD_BEGIN_DECLS
ssize_t strfmon(char *__restrict, size_t, const char *__restrict, ...);
ssize_t strfmon_l(char *__restrict, size_t, locale_t, const char *__restrict, ...);
__SPFXD_END_DECLS
#endif
