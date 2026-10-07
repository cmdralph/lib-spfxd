/* lib-spfxd — <iconv.h> */
#ifndef _ICONV_H
#define _ICONV_H
#include <features.h>
#define __SPFXD_NEED_size_t
#include <bits/typedefs.h>

__SPFXD_BEGIN_DECLS

typedef void *iconv_t;

iconv_t iconv_open(const char *, const char *);
size_t iconv(iconv_t, char **__restrict, size_t *__restrict, char **__restrict, size_t *__restrict);
int iconv_close(iconv_t);

__SPFXD_END_DECLS
#endif
