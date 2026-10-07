/*
 * lib-spfxd — allocator internals ("spfxd-alloc").
 *
 * Design summary
 * --------------
 * Memory comes from the kernel in 4 MiB *segments* aligned to 4 MiB.  The
 * first few pages of a segment hold its header: a page->span map and one
 * span descriptor per page.  The rest of the segment is divided into
 * *spans* (runs of 4 KiB pages) managed by a page heap with coalescing.
 *
 *   free(p): seg  = p & ~(4 MiB - 1)           one AND
 *            span = seg->spans[seg->page_span[(p - seg) >> 12]]
 *
 * so no per-object header exists and small objects carry zero metadata
 * overhead.  Allocations are served three ways:
 *
 *   small  (<= 32 KiB)  40 size classes (16-byte steps to 128, then four
 *                       classes per power of two).  A small span holds
 *                       objects of one class.  Never-used objects are handed
 *                       out from a bump pointer, so untouched pages of a new
 *                       span are never faulted in.  Freed objects form an
 *                       intrusive singly linked list inside the span.
 *   large  (<= 2 MiB)   a dedicated span of whole pages; shrinks and grows
 *                       in place when the neighbouring span is free.
 *   huge   (> 2 MiB)    a private mapping whose first page is a header at a
 *                       4 MiB-aligned address; realloc moves pages with
 *                       mremap instead of copying.
 *
 * Each thread owns a *tcache*: per-class LIFO stacks of free objects.  The
 * common malloc/free pair touches only thread-local data.  Stacks are
 * refilled from / drained to the shared per-class state in batches under a
 * per-class lock; the page heap has its own lock.  All locks are elided
 * while the process is single-threaded.
 *
 * Hardening: free-list links are stored XOR-mangled with the slot address
 * and a per-process secret and validated when popped; segment headers carry
 * a secret-derived magic; frees of misaligned or unknown pointers, and
 * immediate double frees, abort the process.
 *
 * Over-aligned pointers: no small or large allocation can start at offset 0
 * of a segment (the header lives there).  Huge allocations with alignment
 * >= 4 MiB are the only pointers with offset 0; their header is placed in
 * the page right below the pointer instead.
 */
#ifndef _SPFXD_MALLOC_IMPL_H
#define _SPFXD_MALLOC_IMPL_H

#include <stddef.h>
#include <stdint.h>
#include "libc.h"

#define PAGE_SHIFT     12
#define PAGE_SZ        ((size_t)1 << PAGE_SHIFT)
#define SEG_SHIFT      22
#define SEG_SIZE       ((size_t)1 << SEG_SHIFT)
#define SEG_PAGES      (SEG_SIZE >> PAGE_SHIFT)

#define NCLASS         40
#define SMALL_MAX      32768
#define HUGE_THRESHOLD ((size_t)2 << 20)

enum { SPAN_FREE = 0, SPAN_SMALL = 1, SPAN_LARGE = 2, SPAN_HDR = 3 };
enum { SEG_NORMAL = 1, SEG_HUGE = 2 };

struct span {
	struct span *next, *prev;   /* class partial list / page-heap bin list */
	void *free;                 /* mangled free-list head (small spans) */
	uint32_t bump;              /* byte offset of the first never-used object */
	uint16_t npages;
	uint16_t used;              /* objects currently allocated */
	uint16_t capacity;          /* objects that fit in the span */
	uint8_t kind;
	uint8_t cls;
	uint8_t dirty;              /* free spans: pages may hold nonzero data */
	uint8_t in_list;            /* linked into a partial list or a bin */
	uint16_t start;             /* index of the span's first page */
};

struct segment {
	uintptr_t magic;
	uint32_t kind;
	uint32_t hdr_pages;
	size_t map_len;             /* huge: whole mapping; normal: SEG_SIZE */
	size_t off;                 /* huge: user pointer offset from header */
	struct segment *next;
	uint16_t page_span[SEG_PAGES];
	struct span spans[SEG_PAGES];
};

#define HDR_PAGES ((sizeof(struct segment) + PAGE_SZ - 1) / PAGE_SZ)
#define SEG_USABLE (SEG_PAGES - HDR_PAGES)

struct tcache_bin {
	void *head;                 /* mangled LIFO of free objects */
	uint32_t count;
	uint32_t max;
};

struct malloc_tcache {
	struct tcache_bin bin[NCLASS];
};

extern hidden const uint32_t __malloc_class_size[NCLASS];
extern hidden const uint8_t __malloc_class_pages[NCLASS];
extern hidden const uint64_t __malloc_class_magic[NCLASS];

static __inline unsigned size_to_class(size_t n)
{
	if (n <= 128) return n ? (unsigned)((n + 15) >> 4) - 1 : 0;
	size_t s = n - 1;
	unsigned e = 63 - (unsigned)__builtin_clzl(s);
	return 8 + (e - 7) * 4 + (unsigned)((s >> (e - 2)) & 3);
}

#endif
