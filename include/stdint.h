/* lib-spfxd — <stdint.h> (LP64) */
#ifndef _STDINT_H
#define _STDINT_H

#define __SPFXD_NEED_intN_t
#define __SPFXD_NEED_intptr_t
#define __SPFXD_NEED_uintptr_t
#define __SPFXD_NEED_intmax_t
#include <bits/typedefs.h>

typedef int8_t   int_least8_t;
typedef int16_t  int_least16_t;
typedef int32_t  int_least32_t;
typedef int64_t  int_least64_t;
typedef uint8_t  uint_least8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t;
typedef uint64_t uint_least64_t;

/* 32-bit arithmetic is as fast as 64-bit on x86-64 and uses less memory. */
typedef int8_t   int_fast8_t;
typedef int32_t  int_fast16_t;
typedef int32_t  int_fast32_t;
typedef int64_t  int_fast64_t;
typedef uint8_t  uint_fast8_t;
typedef uint32_t uint_fast16_t;
typedef uint32_t uint_fast32_t;
typedef uint64_t uint_fast64_t;

#define INT8_MIN   (-1-0x7f)
#define INT16_MIN  (-1-0x7fff)
#define INT32_MIN  (-1-0x7fffffff)
#define INT64_MIN  (-1-0x7fffffffffffffffL)
#define INT8_MAX   (0x7f)
#define INT16_MAX  (0x7fff)
#define INT32_MAX  (0x7fffffff)
#define INT64_MAX  (0x7fffffffffffffffL)
#define UINT8_MAX  (0xff)
#define UINT16_MAX (0xffff)
#define UINT32_MAX (0xffffffffU)
#define UINT64_MAX (0xffffffffffffffffUL)

#define INT_LEAST8_MIN   INT8_MIN
#define INT_LEAST16_MIN  INT16_MIN
#define INT_LEAST32_MIN  INT32_MIN
#define INT_LEAST64_MIN  INT64_MIN
#define INT_LEAST8_MAX   INT8_MAX
#define INT_LEAST16_MAX  INT16_MAX
#define INT_LEAST32_MAX  INT32_MAX
#define INT_LEAST64_MAX  INT64_MAX
#define UINT_LEAST8_MAX  UINT8_MAX
#define UINT_LEAST16_MAX UINT16_MAX
#define UINT_LEAST32_MAX UINT32_MAX
#define UINT_LEAST64_MAX UINT64_MAX

#define INT_FAST8_MIN   INT8_MIN
#define INT_FAST16_MIN  INT32_MIN
#define INT_FAST32_MIN  INT32_MIN
#define INT_FAST64_MIN  INT64_MIN
#define INT_FAST8_MAX   INT8_MAX
#define INT_FAST16_MAX  INT32_MAX
#define INT_FAST32_MAX  INT32_MAX
#define INT_FAST64_MAX  INT64_MAX
#define UINT_FAST8_MAX  UINT8_MAX
#define UINT_FAST16_MAX UINT32_MAX
#define UINT_FAST32_MAX UINT32_MAX
#define UINT_FAST64_MAX UINT64_MAX

#define INTPTR_MIN  INT64_MIN
#define INTPTR_MAX  INT64_MAX
#define UINTPTR_MAX UINT64_MAX
#define INTMAX_MIN  INT64_MIN
#define INTMAX_MAX  INT64_MAX
#define UINTMAX_MAX UINT64_MAX
#define PTRDIFF_MIN INT64_MIN
#define PTRDIFF_MAX INT64_MAX
#define SIZE_MAX    UINT64_MAX
#define SIG_ATOMIC_MIN INT32_MIN
#define SIG_ATOMIC_MAX INT32_MAX
#define WCHAR_MIN   __WCHAR_MIN__
#define WCHAR_MAX   __WCHAR_MAX__
#define WINT_MIN    0U
#define WINT_MAX    UINT32_MAX

#define INT8_C(c)   c
#define INT16_C(c)  c
#define INT32_C(c)  c
#define INT64_C(c)  c ## L
#define UINT8_C(c)  c
#define UINT16_C(c) c
#define UINT32_C(c) c ## U
#define UINT64_C(c) c ## UL
#define INTMAX_C(c)  c ## L
#define UINTMAX_C(c) c ## UL

#if defined(__STDC_VERSION__) && __STDC_VERSION__ > 201710L
#define INT8_WIDTH 8
#define INT16_WIDTH 16
#define INT32_WIDTH 32
#define INT64_WIDTH 64
#define UINT8_WIDTH 8
#define UINT16_WIDTH 16
#define UINT32_WIDTH 32
#define UINT64_WIDTH 64
#define INTPTR_WIDTH 64
#define UINTPTR_WIDTH 64
#define INTMAX_WIDTH 64
#define UINTMAX_WIDTH 64
#define SIZE_WIDTH 64
#define PTRDIFF_WIDTH 64
#endif

#endif
