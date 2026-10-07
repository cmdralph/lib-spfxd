/* lib-spfxd — <ctype.h>
 *
 * Classification is table driven: a single 384-entry table indexed by
 * c + 128 (so that EOF and negative plain-char values are valid indices)
 * holds one bit per class.  Only the C/POSIX classification exists for
 * single-byte characters; UTF-8 locales classify bytes >= 0x80 as nothing,
 * which is the only correct answer for a multibyte encoding.
 */
#ifndef _CTYPE_H
#define _CTYPE_H
#include <features.h>

#if defined(__SPFXD_POSIX)
#define __SPFXD_NEED_locale_t
#include <bits/typedefs.h>
#endif

__SPFXD_BEGIN_DECLS

int isalnum(int);
int isalpha(int);
int isblank(int);
int iscntrl(int);
int isdigit(int);
int isgraph(int);
int islower(int);
int isprint(int);
int ispunct(int);
int isspace(int);
int isupper(int);
int isxdigit(int);
int tolower(int);
int toupper(int);

extern const unsigned short __spfxd_ctype_tab[384];
#define __SPFXD_CT_UPPER 0x001
#define __SPFXD_CT_LOWER 0x002
#define __SPFXD_CT_DIGIT 0x004
#define __SPFXD_CT_SPACE 0x008
#define __SPFXD_CT_PUNCT 0x010
#define __SPFXD_CT_CNTRL 0x020
#define __SPFXD_CT_XDIGIT 0x040
#define __SPFXD_CT_BLANK 0x080
#define __SPFXD_CT_PRINT 0x100
#define __SPFXD_CT_ALPHA 0x200
#define __SPFXD_CT_GRAPH 0x400
#define __SPFXD_CT_ALNUM 0x800

#ifndef __cplusplus
#define __spfxd_ct(c, m) ((int)(__spfxd_ctype_tab[(int)(c) + 128] & (m)) != 0)
#define isalnum(c)  __spfxd_ct(c, __SPFXD_CT_ALNUM)
#define isalpha(c)  __spfxd_ct(c, __SPFXD_CT_ALPHA)
#define isblank(c)  __spfxd_ct(c, __SPFXD_CT_BLANK)
#define iscntrl(c)  __spfxd_ct(c, __SPFXD_CT_CNTRL)
#define isdigit(c)  ((unsigned)(c) - '0' < 10)
#define isgraph(c)  __spfxd_ct(c, __SPFXD_CT_GRAPH)
#define islower(c)  ((unsigned)(c) - 'a' < 26)
#define isprint(c)  ((unsigned)(c) - 0x20 < 0x5f)
#define ispunct(c)  __spfxd_ct(c, __SPFXD_CT_PUNCT)
#define isspace(c)  __spfxd_ct(c, __SPFXD_CT_SPACE)
#define isupper(c)  ((unsigned)(c) - 'A' < 26)
#define isxdigit(c) __spfxd_ct(c, __SPFXD_CT_XDIGIT)
#endif

#if defined(__SPFXD_POSIX)
int isalnum_l(int, locale_t);
int isalpha_l(int, locale_t);
int isblank_l(int, locale_t);
int iscntrl_l(int, locale_t);
int isdigit_l(int, locale_t);
int isgraph_l(int, locale_t);
int islower_l(int, locale_t);
int isprint_l(int, locale_t);
int ispunct_l(int, locale_t);
int isspace_l(int, locale_t);
int isupper_l(int, locale_t);
int isxdigit_l(int, locale_t);
int tolower_l(int, locale_t);
int toupper_l(int, locale_t);
#endif
#if defined(__SPFXD_XSI) || defined(__SPFXD_BSD)
int isascii(int);
int toascii(int);
#define _tolower(c) tolower(c)
#define _toupper(c) toupper(c)
#endif

__SPFXD_END_DECLS
#endif
