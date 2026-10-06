/* lib-spfxd — <wctype.h>.  Classification and case mapping follow the
 * Unicode Character Database (see tools/gen-unicode.py) independent of the
 * current locale. */
#ifndef _WCTYPE_H
#define _WCTYPE_H
#include <features.h>

#define __SPFXD_NEED_wint_t
#define __SPFXD_NEED_wctype_t
#define __SPFXD_NEED_wctrans_t
#if defined(__SPFXD_POSIX)
#define __SPFXD_NEED_locale_t
#endif
#include <bits/typedefs.h>

#ifndef WEOF
#define WEOF 0xffffffffU
#endif

__SPFXD_BEGIN_DECLS
int iswalnum(wint_t);
int iswalpha(wint_t);
int iswblank(wint_t);
int iswcntrl(wint_t);
int iswdigit(wint_t);
int iswgraph(wint_t);
int iswlower(wint_t);
int iswprint(wint_t);
int iswpunct(wint_t);
int iswspace(wint_t);
int iswupper(wint_t);
int iswxdigit(wint_t);
int iswctype(wint_t, wctype_t);
wctype_t wctype(const char *);
wint_t towlower(wint_t);
wint_t towupper(wint_t);
wint_t towctrans(wint_t, wctrans_t);
wctrans_t wctrans(const char *);
#if defined(__SPFXD_POSIX)
int iswalnum_l(wint_t, locale_t);
int iswalpha_l(wint_t, locale_t);
int iswblank_l(wint_t, locale_t);
int iswcntrl_l(wint_t, locale_t);
int iswdigit_l(wint_t, locale_t);
int iswgraph_l(wint_t, locale_t);
int iswlower_l(wint_t, locale_t);
int iswprint_l(wint_t, locale_t);
int iswpunct_l(wint_t, locale_t);
int iswspace_l(wint_t, locale_t);
int iswupper_l(wint_t, locale_t);
int iswxdigit_l(wint_t, locale_t);
int iswctype_l(wint_t, wctype_t, locale_t);
wint_t towlower_l(wint_t, locale_t);
wint_t towupper_l(wint_t, locale_t);
wctype_t wctype_l(const char *, locale_t);
wint_t towctrans_l(wint_t, wctrans_t, locale_t);
wctrans_t wctrans_l(const char *, locale_t);
#endif
__SPFXD_END_DECLS
#endif
