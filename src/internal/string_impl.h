/*
 * lib-spfxd — helpers for word-at-a-time string scanning.
 *
 * A `word` load through a may_alias type is legal for any object type.
 * Scanning code only performs aligned word loads past the first unaligned
 * prefix, and an aligned word never straddles a page boundary, so reading
 * a whole word that contains the terminator can never fault even when the
 * bytes after the terminator are outside the object.
 *
 * HASZERO(x) is nonzero iff some byte of x is zero: subtracting 0x01 from
 * every byte borrows out of exactly those bytes that were 0x00 (or that
 * received a borrow), and `& ~x & 0x80..80` discards bytes whose top bit was
 * already set.  The lowest flagged byte is always a true zero byte, which
 * is all the callers rely on (they locate it with ctz).
 */
#ifndef _SPFXD_STRING_IMPL_H
#define _SPFXD_STRING_IMPL_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

typedef size_t __attribute__((__may_alias__)) word_t;
#define WSIZE sizeof(size_t)
#define ONES ((size_t)-1 / UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(x) (((x) - ONES) & ~(x) & HIGHS)
#define ALIGNED(p) (((uintptr_t)(p) & (WSIZE - 1)) == 0)

/* Index (in bytes, little-endian) of the first zero byte flagged by HASZERO. */
static __inline size_t __zero_byte_index(size_t z)
{
	return (size_t)__builtin_ctzl(z) >> 3;
}

#endif

#include "libc.h"
hidden char *__substr_search(const unsigned char *h, size_t avail, int strmode,
	const unsigned char *n, size_t m, int fold);
