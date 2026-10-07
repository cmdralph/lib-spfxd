/*
 * lib-spfxd — <features.h>
 *
 * Feature-test macro processing.  Public headers consult the internal
 * __SPFXD_POSIX / __SPFXD_XSI / __SPFXD_BSD / __SPFXD_GNU switches.
 *
 *   -D_GNU_SOURCE                 everything, including GNU extensions
 *   -D_DEFAULT_SOURCE/_BSD_SOURCE POSIX.1-2008 + XSI + common BSD interfaces
 *   -D_XOPEN_SOURCE=700           POSIX + XSI
 *   -D_POSIX_C_SOURCE=200809L     POSIX only
 *   strict ISO mode (-std=c11)    ISO C only
 *   nothing specified, GNU mode   same as _DEFAULT_SOURCE
 */
#ifndef _FEATURES_H
#define _FEATURES_H

#define __SPFXD__ 1
#define __SPFXD_VERSION_MAJOR 1
#define __SPFXD_VERSION_MINOR 0

#if defined(_ALL_SOURCE) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif

#if defined(_GNU_SOURCE)
# define __SPFXD_GNU 1
# define __SPFXD_BSD 1
# define __SPFXD_XSI 1
# define __SPFXD_POSIX 1
#elif defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE) || \
      (!defined(__STRICT_ANSI__) && !defined(_POSIX_SOURCE) && \
       !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE))
# define __SPFXD_BSD 1
# define __SPFXD_XSI 1
# define __SPFXD_POSIX 1
#else
# if defined(_XOPEN_SOURCE)
#  define __SPFXD_XSI 1
#  define __SPFXD_POSIX 1
# elif defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE)
#  define __SPFXD_POSIX 1
# endif
#endif

#if defined(__cplusplus)
# define __SPFXD_BEGIN_DECLS extern "C" {
# define __SPFXD_END_DECLS }
# define __restrict __restrict__
# define __spfxd_noreturn [[noreturn]]
# if __cplusplus >= 201103L
#  define __spfxd_nothrow noexcept(true)
# else
#  define __spfxd_nothrow throw()
# endif
#else
# define __SPFXD_BEGIN_DECLS
# define __SPFXD_END_DECLS
# if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#  define __restrict restrict
# else
#  define __restrict
# endif
# if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define __spfxd_noreturn _Noreturn
# else
#  define __spfxd_noreturn __attribute__((__noreturn__))
# endif
# define __spfxd_nothrow
#endif

#if defined(__cplusplus) || !defined(__STDC_VERSION__) || __STDC_VERSION__ >= 199901L
# define __inline inline
#endif

#define __spfxd_pure      __attribute__((__pure__))
#define __spfxd_const     __attribute__((__const__))
#define __spfxd_malloc    __attribute__((__malloc__))
#define __spfxd_nonnull(...) __attribute__((__nonnull__(__VA_ARGS__)))
#define __spfxd_printf(f, a) __attribute__((__format__(__printf__, f, a)))
#define __spfxd_scanf(f, a)  __attribute__((__format__(__scanf__, f, a)))
#define __spfxd_alloc_size(...) __attribute__((__alloc_size__(__VA_ARGS__)))
#define __spfxd_warn_unused __attribute__((__warn_unused_result__))
#define __spfxd_deprecated __attribute__((__deprecated__))

/* C11 library features are always visible except in strict pre-C11 modes. */
#if defined(__cplusplus) || !defined(__STRICT_ANSI__) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
# define __SPFXD_C11 1
#endif

#endif
