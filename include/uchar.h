/* lib-spfxd — <uchar.h> */
#ifndef _UCHAR_H
#define _UCHAR_H
#include <features.h>

#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_mbstate_t
#include <bits/typedefs.h>

#ifndef __cplusplus
typedef unsigned short char16_t;
typedef unsigned int char32_t;
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ > 201710L && !defined(__cplusplus)
typedef unsigned char char8_t;
#endif

__SPFXD_BEGIN_DECLS
size_t c16rtomb(char *__restrict, char16_t, mbstate_t *__restrict);
size_t mbrtoc16(char16_t *__restrict, const char *__restrict, size_t, mbstate_t *__restrict);
size_t c32rtomb(char *__restrict, char32_t, mbstate_t *__restrict);
size_t mbrtoc32(char32_t *__restrict, const char *__restrict, size_t, mbstate_t *__restrict);
__SPFXD_END_DECLS
#endif
