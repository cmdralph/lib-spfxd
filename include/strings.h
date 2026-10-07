/* lib-spfxd — <strings.h> */
#ifndef _STRINGS_H
#define _STRINGS_H
#include <features.h>

#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_locale_t
#include <bits/typedefs.h>

__SPFXD_BEGIN_DECLS
int ffs(int) __spfxd_const;
int ffsl(long) __spfxd_const;
int ffsll(long long) __spfxd_const;
int strcasecmp(const char *, const char *) __spfxd_pure;
int strncasecmp(const char *, const char *, size_t) __spfxd_pure;
int strcasecmp_l(const char *, const char *, locale_t);
int strncasecmp_l(const char *, const char *, size_t, locale_t);
#if defined(__SPFXD_BSD) || defined(__SPFXD_XSI)
int bcmp(const void *, const void *, size_t) __spfxd_pure;
void bcopy(const void *, void *, size_t);
void bzero(void *, size_t);
char *index(const char *, int) __spfxd_pure;
char *rindex(const char *, int) __spfxd_pure;
#endif
__SPFXD_END_DECLS
#endif
