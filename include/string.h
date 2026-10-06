/* lib-spfxd — <string.h> */
#ifndef _STRING_H
#define _STRING_H
#include <features.h>

#define __SPFXD_NEED_size_t
#if defined(__SPFXD_POSIX)
#define __SPFXD_NEED_locale_t
#endif
#include <bits/typedefs.h>

#ifdef __cplusplus
# define NULL 0L
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif

__SPFXD_BEGIN_DECLS

void *memcpy(void *__restrict, const void *__restrict, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t) __spfxd_pure;
void *memchr(const void *, int, size_t) __spfxd_pure;

char *strcpy(char *__restrict, const char *__restrict);
char *strncpy(char *__restrict, const char *__restrict, size_t);
char *strcat(char *__restrict, const char *__restrict);
char *strncat(char *__restrict, const char *__restrict, size_t);
int strcmp(const char *, const char *) __spfxd_pure;
int strncmp(const char *, const char *, size_t) __spfxd_pure;
int strcoll(const char *, const char *);
size_t strxfrm(char *__restrict, const char *__restrict, size_t);
char *strchr(const char *, int) __spfxd_pure;
char *strrchr(const char *, int) __spfxd_pure;
size_t strcspn(const char *, const char *) __spfxd_pure;
size_t strspn(const char *, const char *) __spfxd_pure;
char *strpbrk(const char *, const char *) __spfxd_pure;
char *strstr(const char *, const char *) __spfxd_pure;
char *strtok(char *__restrict, const char *__restrict);
size_t strlen(const char *) __spfxd_pure;
char *strerror(int);

#if defined(__SPFXD_POSIX)
char *strtok_r(char *__restrict, const char *__restrict, char **__restrict);
#if defined(__SPFXD_GNU)
char *strerror_r(int, char *, size_t);
#else
int strerror_r(int, char *, size_t) __asm__("__xpg_strerror_r");
#endif
int __xpg_strerror_r(int, char *, size_t);
char *stpcpy(char *__restrict, const char *__restrict);
char *stpncpy(char *__restrict, const char *__restrict, size_t);
size_t strnlen(const char *, size_t) __spfxd_pure;
char *strdup(const char *) __spfxd_malloc;
char *strndup(const char *, size_t) __spfxd_malloc;
char *strsignal(int);
char *strerror_l(int, locale_t);
int strcoll_l(const char *, const char *, locale_t);
size_t strxfrm_l(char *__restrict, const char *__restrict, size_t, locale_t);
void *memccpy(void *__restrict, const void *__restrict, int, size_t);
#endif

#if defined(__SPFXD_BSD)
char *strsep(char **, const char *);
size_t strlcat(char *, const char *, size_t);
size_t strlcpy(char *, const char *, size_t);
void explicit_bzero(void *, size_t);
#include <strings.h>
#endif

#if defined(__SPFXD_GNU)
#define strdupa(s) __extension__ ({ const char *__s = (s); \
	unsigned long __n = strlen(__s) + 1; \
	(char *)memcpy(__builtin_alloca(__n), __s, __n); })
int strverscmp(const char *, const char *);
char *strchrnul(const char *, int) __spfxd_pure;
char *strcasestr(const char *, const char *) __spfxd_pure;
void *memmem(const void *, size_t, const void *, size_t) __spfxd_pure;
void *memrchr(const void *, int, size_t) __spfxd_pure;
void *rawmemchr(const void *, int) __spfxd_pure;
void *mempcpy(void *__restrict, const void *__restrict, size_t);
char *basename(const char *);
const char *sigdescr_np(int);
const char *sigabbrev_np(int);
const char *strerrorname_np(int);
const char *strerrordesc_np(int);
#endif

__SPFXD_END_DECLS
#endif
