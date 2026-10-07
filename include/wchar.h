/* lib-spfxd — <wchar.h> */
#ifndef _WCHAR_H
#define _WCHAR_H
#include <features.h>

#define __SPFXD_NEED_FILE
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_wchar_t
#define __SPFXD_NEED_wint_t
#define __SPFXD_NEED_mbstate_t
#define __SPFXD_NEED___va_list
#if defined(__SPFXD_POSIX)
#define __SPFXD_NEED_locale_t
#define __SPFXD_NEED_va_list
#endif
#if defined(__SPFXD_XSI)
#define __SPFXD_NEED_wctype_t
#endif
#include <bits/typedefs.h>

#ifdef __cplusplus
# define NULL 0L
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif

#define WCHAR_MIN __WCHAR_MIN__
#define WCHAR_MAX __WCHAR_MAX__
#define WEOF 0xffffffffU

__SPFXD_BEGIN_DECLS
struct tm;

wchar_t *wcscpy(wchar_t *__restrict, const wchar_t *__restrict);
wchar_t *wcsncpy(wchar_t *__restrict, const wchar_t *__restrict, size_t);
wchar_t *wcscat(wchar_t *__restrict, const wchar_t *__restrict);
wchar_t *wcsncat(wchar_t *__restrict, const wchar_t *__restrict, size_t);
int wcscmp(const wchar_t *, const wchar_t *);
int wcsncmp(const wchar_t *, const wchar_t *, size_t);
int wcscoll(const wchar_t *, const wchar_t *);
size_t wcsxfrm(wchar_t *__restrict, const wchar_t *__restrict, size_t);
wchar_t *wcschr(const wchar_t *, wchar_t);
wchar_t *wcsrchr(const wchar_t *, wchar_t);
size_t wcscspn(const wchar_t *, const wchar_t *);
size_t wcsspn(const wchar_t *, const wchar_t *);
wchar_t *wcspbrk(const wchar_t *, const wchar_t *);
wchar_t *wcstok(wchar_t *__restrict, const wchar_t *__restrict, wchar_t **__restrict);
size_t wcslen(const wchar_t *);
wchar_t *wcsstr(const wchar_t *__restrict, const wchar_t *__restrict);
wchar_t *wmemchr(const wchar_t *, wchar_t, size_t);
int wmemcmp(const wchar_t *, const wchar_t *, size_t);
wchar_t *wmemcpy(wchar_t *__restrict, const wchar_t *__restrict, size_t);
wchar_t *wmemmove(wchar_t *, const wchar_t *, size_t);
wchar_t *wmemset(wchar_t *, wchar_t, size_t);

wint_t btowc(int);
int wctob(wint_t);
int mbsinit(const mbstate_t *);
size_t mbrtowc(wchar_t *__restrict, const char *__restrict, size_t, mbstate_t *__restrict);
size_t wcrtomb(char *__restrict, wchar_t, mbstate_t *__restrict);
size_t mbrlen(const char *__restrict, size_t, mbstate_t *__restrict);
size_t mbsrtowcs(wchar_t *__restrict, const char **__restrict, size_t, mbstate_t *__restrict);
size_t wcsrtombs(char *__restrict, const wchar_t **__restrict, size_t, mbstate_t *__restrict);

float wcstof(const wchar_t *__restrict, wchar_t **__restrict);
double wcstod(const wchar_t *__restrict, wchar_t **__restrict);
long double wcstold(const wchar_t *__restrict, wchar_t **__restrict);
long wcstol(const wchar_t *__restrict, wchar_t **__restrict, int);
unsigned long wcstoul(const wchar_t *__restrict, wchar_t **__restrict, int);
long long wcstoll(const wchar_t *__restrict, wchar_t **__restrict, int);
unsigned long long wcstoull(const wchar_t *__restrict, wchar_t **__restrict, int);

int fwide(FILE *, int);
int wprintf(const wchar_t *__restrict, ...);
int fwprintf(FILE *__restrict, const wchar_t *__restrict, ...);
int swprintf(wchar_t *__restrict, size_t, const wchar_t *__restrict, ...);
int vwprintf(const wchar_t *__restrict, __spfxd_va_list);
int vfwprintf(FILE *__restrict, const wchar_t *__restrict, __spfxd_va_list);
int vswprintf(wchar_t *__restrict, size_t, const wchar_t *__restrict, __spfxd_va_list);
int wscanf(const wchar_t *__restrict, ...);
int fwscanf(FILE *__restrict, const wchar_t *__restrict, ...);
int swscanf(const wchar_t *__restrict, const wchar_t *__restrict, ...);
int vwscanf(const wchar_t *__restrict, __spfxd_va_list);
int vfwscanf(FILE *__restrict, const wchar_t *__restrict, __spfxd_va_list);
int vswscanf(const wchar_t *__restrict, const wchar_t *__restrict, __spfxd_va_list);
wint_t fgetwc(FILE *);
wint_t getwc(FILE *);
wint_t getwchar(void);
wint_t fputwc(wchar_t, FILE *);
wint_t putwc(wchar_t, FILE *);
wint_t putwchar(wchar_t);
wchar_t *fgetws(wchar_t *__restrict, int, FILE *__restrict);
int fputws(const wchar_t *__restrict, FILE *__restrict);
wint_t ungetwc(wint_t, FILE *);
size_t wcsftime(wchar_t *__restrict, size_t, const wchar_t *__restrict, const struct tm *__restrict);

#if defined(__SPFXD_POSIX)
size_t mbsnrtowcs(wchar_t *__restrict, const char **__restrict, size_t, size_t, mbstate_t *__restrict);
size_t wcsnrtombs(char *__restrict, const wchar_t **__restrict, size_t, size_t, mbstate_t *__restrict);
wchar_t *wcsdup(const wchar_t *);
size_t wcsnlen(const wchar_t *, size_t);
wchar_t *wcpcpy(wchar_t *__restrict, const wchar_t *__restrict);
wchar_t *wcpncpy(wchar_t *__restrict, const wchar_t *__restrict, size_t);
int wcscasecmp(const wchar_t *, const wchar_t *);
int wcsncasecmp(const wchar_t *, const wchar_t *, size_t);
FILE *open_wmemstream(wchar_t **, size_t *);
#endif
#if defined(__SPFXD_XSI)
int wcwidth(wchar_t);
int wcswidth(const wchar_t *, size_t);
#endif
#if defined(__SPFXD_GNU)
wint_t fgetwc_unlocked(FILE *);
wint_t fputwc_unlocked(wchar_t, FILE *);
wint_t getwc_unlocked(FILE *);
wint_t putwc_unlocked(wchar_t, FILE *);
wchar_t *wcschrnul(const wchar_t *, wchar_t);
wchar_t *wmempcpy(wchar_t *__restrict, const wchar_t *__restrict, size_t);
#endif

__SPFXD_END_DECLS
#endif
