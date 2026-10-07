/* lib-spfxd — <err.h> (BSD diagnostics) */
#ifndef _ERR_H
#define _ERR_H
#include <features.h>
#define __SPFXD_NEED___va_list
#include <bits/typedefs.h>
__SPFXD_BEGIN_DECLS
void warn(const char *, ...) __spfxd_printf(1, 2);
void vwarn(const char *, __spfxd_va_list) __spfxd_printf(1, 0);
void warnx(const char *, ...) __spfxd_printf(1, 2);
void vwarnx(const char *, __spfxd_va_list) __spfxd_printf(1, 0);
__spfxd_noreturn void err(int, const char *, ...) __spfxd_printf(2, 3);
__spfxd_noreturn void verr(int, const char *, __spfxd_va_list) __spfxd_printf(2, 0);
__spfxd_noreturn void errx(int, const char *, ...) __spfxd_printf(2, 3);
__spfxd_noreturn void verrx(int, const char *, __spfxd_va_list) __spfxd_printf(2, 0);
__SPFXD_END_DECLS
#endif
