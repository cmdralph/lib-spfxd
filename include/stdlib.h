/* lib-spfxd — <stdlib.h> */
#ifndef _STDLIB_H
#define _STDLIB_H
#include <features.h>

#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_wchar_t
#if defined(__SPFXD_GNU)
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

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 0x7fffffff
#define MB_CUR_MAX ((size_t)__spfxd_mb_cur_max())

typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
typedef struct { long long quot, rem; } lldiv_t;

__SPFXD_BEGIN_DECLS

size_t __spfxd_mb_cur_max(void);

int atoi(const char *);
long atol(const char *);
long long atoll(const char *);
double atof(const char *);
long strtol(const char *__restrict, char **__restrict, int);
long long strtoll(const char *__restrict, char **__restrict, int);
unsigned long strtoul(const char *__restrict, char **__restrict, int);
unsigned long long strtoull(const char *__restrict, char **__restrict, int);
float strtof(const char *__restrict, char **__restrict);
double strtod(const char *__restrict, char **__restrict);
long double strtold(const char *__restrict, char **__restrict);

int rand(void);
void srand(unsigned);

void *malloc(size_t) __spfxd_malloc __spfxd_alloc_size(1);
void *calloc(size_t, size_t) __spfxd_malloc __spfxd_alloc_size(1, 2);
void *realloc(void *, size_t) __spfxd_alloc_size(2);
void free(void *);
#if defined(__SPFXD_C11)
void *aligned_alloc(size_t, size_t) __spfxd_malloc __spfxd_alloc_size(2);
#endif

__spfxd_noreturn void abort(void);
int atexit(void (*)(void));
__spfxd_noreturn void exit(int);
__spfxd_noreturn void _Exit(int);
#if defined(__SPFXD_C11)
int at_quick_exit(void (*)(void));
__spfxd_noreturn void quick_exit(int);
#endif

char *getenv(const char *);
int system(const char *);

void *bsearch(const void *, const void *, size_t, size_t, int (*)(const void *, const void *));
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));

int abs(int) __spfxd_const;
long labs(long) __spfxd_const;
long long llabs(long long) __spfxd_const;
div_t div(int, int) __spfxd_const;
ldiv_t ldiv(long, long) __spfxd_const;
lldiv_t lldiv(long long, long long) __spfxd_const;

int mblen(const char *, size_t);
int mbtowc(wchar_t *__restrict, const char *__restrict, size_t);
int wctomb(char *, wchar_t);
size_t mbstowcs(wchar_t *__restrict, const char *__restrict, size_t);
size_t wcstombs(char *__restrict, const wchar_t *__restrict, size_t);

#if defined(__SPFXD_POSIX)
int posix_memalign(void **, size_t, size_t);
int setenv(const char *, const char *, int);
int unsetenv(const char *);
int rand_r(unsigned *);
char *mkdtemp(char *);
int mkstemp(char *);
int getsubopt(char **, char *const *, char **);
#endif

#if defined(__SPFXD_XSI)
#define __SPFXD_STDLIB_XSI
int putenv(char *);
long random(void);
void srandom(unsigned);
char *initstate(unsigned, char *, size_t);
char *setstate(char *);
double drand48(void);
double erand48(unsigned short[3]);
long lrand48(void);
long nrand48(unsigned short[3]);
long mrand48(void);
long jrand48(unsigned short[3]);
void srand48(long);
unsigned short *seed48(unsigned short[3]);
void lcong48(unsigned short[7]);
char *l64a(long);
long a64l(const char *);
int grantpt(int);
int unlockpt(int);
char *ptsname(int);
int posix_openpt(int);
char *realpath(const char *__restrict, char *__restrict);
char *ecvt(double, int, int *, int *);
char *fcvt(double, int, int *, int *);
char *gcvt(double, int, char *);
#endif

#if defined(__SPFXD_BSD)
void *reallocarray(void *, size_t, size_t);
void *valloc(size_t);
void *memalign(size_t, size_t);
int clearenv(void);
int mkostemp(char *, int);
int mkstemps(char *, int);
int mkostemps(char *, int, int);
int getloadavg(double *, int);
unsigned arc4random(void);
void arc4random_buf(void *, size_t);
unsigned arc4random_uniform(unsigned);
#endif

#if defined(__SPFXD_GNU)
char *secure_getenv(const char *);
int ptsname_r(int, char *, size_t);
char *canonicalize_file_name(const char *);
float strtof_l(const char *__restrict, char **__restrict, locale_t);
double strtod_l(const char *__restrict, char **__restrict, locale_t);
long double strtold_l(const char *__restrict, char **__restrict, locale_t);
typedef int (*__compar_d_fn_t)(const void *, const void *, void *);
void qsort_r(void *, size_t, size_t, int (*)(const void *, const void *, void *), void *);
#endif

__SPFXD_END_DECLS
#endif
