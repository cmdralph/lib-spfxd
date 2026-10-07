/*
 * lib-spfxd — locales.
 *
 * The library implements the C/POSIX locale and, for LC_CTYPE, a UTF-8
 * variant of it (selected by any locale name ending in ".UTF-8"/".utf8",
 * e.g. "C.UTF-8" or "en_US.UTF-8").  All other categories always behave as
 * in the C locale.  In the plain C locale the multibyte encoding is a
 * single-byte one in which every byte value b maps to the wide character
 * b (so all 256 byte values round-trip, as POSIX.1-2024 requires).
 */
#ifndef _SPFXD_LOCALE_IMPL_H
#define _SPFXD_LOCALE_IMPL_H

#include <locale.h>
#include "libc.h"

#define LOCALE_NAME_MAX 64

struct __spfxd_locale {
	int utf8;                               /* LC_CTYPE is UTF-8 */
	char names[LC_ALL][LOCALE_NAME_MAX];    /* per-category names */
};

extern hidden struct __spfxd_locale __global_locale;
extern hidden const struct __spfxd_locale __c_locale;

/* Locale in effect for the calling thread (uselocale or global). */
hidden const struct __spfxd_locale *__current_locale(void);

static __inline int __locale_utf8(void)
{
	return __current_locale()->utf8;
}

#endif
