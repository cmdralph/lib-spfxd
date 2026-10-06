/*
 * lib-spfxd — the allocator.  See src/internal/malloc_impl.h for the design.
 *
 * Lock order: class lock -> heap lock.  Nothing takes a class lock while
 * holding the heap lock.
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <sys/mman.h>
#include "malloc_impl.h"
#include "pthread_impl.h"
#include "lock.h"

#define SEG_MAGIC 0x5350465844414c4cUL   /* "SPFXDALL" */
#define PURGE_HIGH 2048                  /* dirty free pages tolerated (8 MiB) */
#define PURGE_LOW  512                   /* purge down to this (2 MiB) */
#define NBINS 129                        /* bins 1..127 exact, 128 = 128+ pages */

static volatile int heap_lock;
static struct span *bins[NBINS];
static uint64_t binmap[3];
static struct segment *segments;
static size_t dirty_free_pages;
static int empty_segments;

static struct class_state {
	volatile int lock;
	struct span *partial;
} classes[NCLASS];

static struct malloc_tcache main_tcache;

/* ---------------------------------------------------------------------- */
/* diagnostics                                                             */
/* ---------------------------------------------------------------------- */

static cold __attribute__((__noreturn__)) void malloc_abort(const char *why)
{
	__libc_fatal(why);
}

static __inline uintptr_t seg_magic(void)
{
	return SEG_MAGIC ^ __libc.secret;
}

/* ---------------------------------------------------------------------- */
/* free-list link mangling                                                 */
/* ---------------------------------------------------------------------- */

static __inline void *link_encode(void *slot, void *next)
{
	return (void *)((uintptr_t)next ^ ((uintptr_t)slot >> PAGE_SHIFT) ^ __libc.secret);
}

static __inline void *link_decode(void *slot)
{
	uintptr_t v = (uintptr_t)*(void **)slot ^ ((uintptr_t)slot >> PAGE_SHIFT) ^ __libc.secret;
	if (unlikely(v & 15)) malloc_abort("malloc: corrupted free list");
	return (void *)v;
}

/* ---------------------------------------------------------------------- */
/* raw memory from the kernel                                              */
/* ---------------------------------------------------------------------- */

static void *sys_map(size_t len)
{
	long r = __syscall(SYS_mmap, 0, len, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	return __is_err(r) ? 0 : (void *)r;
}

static void sys_unmap(void *p, size_t len)
{
	if (len) __syscall(SYS_munmap, p, len);
}

/* A mapping of `len` bytes starting at S with (S + phase) % align == 0. */
static void *map_aligned(size_t len, size_t align, size_t phase)
{
	if (len > SIZE_MAX - align) return 0;
	unsigned char *p = sys_map(len + align);
	if (!p) return 0;
	uintptr_t s = (((uintptr_t)p + phase + align - 1) & -(uintptr_t)align) - phase;
	if (s < (uintptr_t)p) s += align;
	sys_unmap(p, s - (uintptr_t)p);
	sys_unmap((void *)(s + len), (uintptr_t)p + len + align - (s + len));
	return (void *)s;
}

/* ---------------------------------------------------------------------- */
/* segments and the page heap (heap_lock held)                             */
/* ---------------------------------------------------------------------- */

static __inline struct segment *seg_of(const void *p)
{
	return (struct segment *)((uintptr_t)p & ~(SEG_SIZE - 1));
}

static __inline unsigned char *span_base(const struct span *s)
{
	return (unsigned char *)seg_of(s) + ((size_t)s->start << PAGE_SHIFT);
}

static __inline struct span *span_of(struct segment *g, const void *p)
{
	unsigned pg = (unsigned)(((uintptr_t)p - (uintptr_t)g) >> PAGE_SHIFT);
	return &g->spans[g->page_span[pg]];
}

static void map_span(struct span *s, int all)
{
	struct segment *g = seg_of(s);
	unsigned st = s->start, n = s->npages;
	if (all) {
		for (unsigned i = st; i < st + n; i++) g->page_span[i] = (uint16_t)st;
	} else {
		g->page_span[st] = (uint16_t)st;
		g->page_span[st + n - 1] = (uint16_t)st;
	}
}

static __inline unsigned bin_index(size_t npages)
{
	return npages < 128 ? (unsigned)npages : 128;
}

static void bin_insert(struct span *s)
{
	unsigned b = bin_index(s->npages);
	s->prev = 0;
	s->next = bins[b];
	if (s->next) s->next->prev = s;
	bins[b] = s;
	s->in_list = 1;
	binmap[b >> 6] |= 1ULL << (b & 63);
	if (s->dirty) dirty_free_pages += s->npages;
	if (s->npages == SEG_USABLE) empty_segments++;
}

static void bin_remove(struct span *s)
{
	unsigned b = bin_index(s->npages);
	if (s->prev) s->prev->next = s->next;
	else bins[b] = s->next;
	if (s->next) s->next->prev = s->prev;
	s->in_list = 0;
	if (!bins[b]) binmap[b >> 6] &= ~(1ULL << (b & 63));
	if (s->dirty) dirty_free_pages -= s->npages;
	if (s->npages == SEG_USABLE) empty_segments--;
}

static struct span *new_segment(void)
{
	struct segment *g = map_aligned(SEG_SIZE, SEG_SIZE, 0);
	if (!g) return 0;
	g->magic = seg_magic();
	g->kind = SEG_NORMAL;
	g->hdr_pages = HDR_PAGES;
	g->map_len = SEG_SIZE;
	g->next = segments;
	segments = g;

	struct span *h = &g->spans[0];
	h->start = 0;
	h->npages = HDR_PAGES;
	h->kind = SPAN_HDR;
	map_span(h, 0);

	struct span *f = &g->spans[HDR_PAGES];
	f->start = HDR_PAGES;
	f->npages = SEG_USABLE;
	f->kind = SPAN_FREE;
	f->dirty = 0;
	map_span(f, 0);
	return f;
}

static void release_segment(struct segment *g)
{
	struct segment **pp = &segments;
	while (*pp && *pp != g) pp = &(*pp)->next;
	if (*pp) *pp = g->next;
	g->magic = 0;
	sys_unmap(g, SEG_SIZE);
}

/* First bin index >= b that is non-empty, or -1. */
static int find_bin(unsigned b)
{
	for (unsigned w = b >> 6; w < 3; w++) {
		uint64_t m = binmap[w];
		if (w == b >> 6) m &= ~0ULL << (b & 63);
		if (m) return (int)(w * 64 + (unsigned)__builtin_ctzll(m));
	}
	return -1;
}

/* Split s so that it keeps its first n pages; the rest becomes a free span. */
static void split_tail(struct span *s, size_t n, int dirty)
{
	struct segment *g = seg_of(s);
	struct span *r = &g->spans[s->start + n];
	r->start = (uint16_t)(s->start + n);
	r->npages = (uint16_t)(s->npages - n);
	r->kind = SPAN_FREE;
	r->dirty = (uint8_t)dirty;
	r->in_list = 0;
	map_span(r, 0);
	s->npages = (uint16_t)n;
	bin_insert(r);
}

static struct span *heap_alloc(size_t n)
{
	struct span *s = 0;
	if (!n || n > SEG_USABLE) return 0;
	int b = find_bin(bin_index(n));
	if (b >= 0 && b < 128) {
		s = bins[b];
	} else if (b == 128) {
		for (struct span *t = bins[128]; t; t = t->next)
			if (t->npages >= n && (!s || t->npages < s->npages)) s = t;
	}
	if (!s) {
		s = new_segment();
		if (!s) return 0;
	} else {
		bin_remove(s);
	}
	if (s->npages > n) split_tail(s, n, s->dirty);
	s->in_list = 0;
	return s;
}

static void purge(size_t target)
{
	for (int b = NBINS - 1; b >= 1 && dirty_free_pages > target; b--) {
		for (struct span *s = bins[b]; s && dirty_free_pages > target; s = s->next) {
			if (!s->dirty) continue;
			__syscall(SYS_madvise, span_base(s), (size_t)s->npages << PAGE_SHIFT, MADV_DONTNEED);
			s->dirty = 0;
			dirty_free_pages -= s->npages;
		}
	}
}

static void heap_free(struct span *s, int dirty)
{
	struct segment *g = seg_of(s);
	s->kind = SPAN_FREE;
	s->dirty = (uint8_t)dirty;

	unsigned end = s->start + s->npages;
	if (end < SEG_PAGES) {
		struct span *nx = &g->spans[end];
		if (nx->kind == SPAN_FREE && nx->in_list) {
			bin_remove(nx);
			nx->in_list = 0;
			s->npages = (uint16_t)(s->npages + nx->npages);
			s->dirty |= nx->dirty;
		}
	}
	if (s->start > HDR_PAGES) {
		struct span *pv = &g->spans[g->page_span[s->start - 1]];
		if (pv->kind == SPAN_FREE && pv->in_list) {
			bin_remove(pv);
			pv->npages = (uint16_t)(pv->npages + s->npages);
			pv->dirty |= s->dirty;
			s = pv;
		}
	}
	map_span(s, 0);

	if (s->npages == SEG_USABLE && empty_segments >= 1) {
		release_segment(g);
		return;
	}
	bin_insert(s);
	if (dirty_free_pages > PURGE_HIGH) purge(PURGE_LOW);
}

/* ---------------------------------------------------------------------- */
/* size classes: central state                                             */
/* ---------------------------------------------------------------------- */

static void partial_insert(struct class_state *c, struct span *s)
{
	s->prev = 0;
	s->next = c->partial;
	if (s->next) s->next->prev = s;
	c->partial = s;
	s->in_list = 1;
}

static void partial_remove(struct class_state *c, struct span *s)
{
	if (s->prev) s->prev->next = s->next;
	else c->partial = s->next;
	if (s->next) s->next->prev = s->prev;
	s->in_list = 0;
}

static struct span *new_small_span(unsigned cls)
{
	size_t pages = __malloc_class_pages[cls];
	__lock(&heap_lock);
	struct span *s = heap_alloc(pages);
	__unlock(&heap_lock);
	if (!s) return 0;
	s->kind = SPAN_SMALL;
	s->cls = (uint8_t)cls;
	s->used = 0;
	s->bump = 0;
	s->free = 0;
	s->capacity = (uint16_t)((pages << PAGE_SHIFT) / __malloc_class_size[cls]);
	map_span(s, 1);
	return s;
}

/* Move up to `want` objects of class cls into the tcache bin.  Objects are
 * pushed so that the bin pops them in increasing address order. */
static unsigned central_fill(unsigned cls, struct tcache_bin *tb, unsigned want)
{
	struct class_state *c = &classes[cls];
	size_t cs = __malloc_class_size[cls];
	unsigned got = 0;

	__lock(&c->lock);
	while (got < want) {
		struct span *s = c->partial;
		if (!s) {
			s = new_small_span(cls);
			if (!s) break;
			partial_insert(c, s);
		}
		/* bump region first (fresh, cache-cold memory is handed out last
		 * in LIFO order, keeping recently freed objects hot) */
		size_t limit = (size_t)s->capacity * cs;
		if (s->bump < limit) {
			unsigned k = (unsigned)((limit - s->bump) / cs);
			if (k > want - got) k = want - got;
			unsigned char *base = span_base(s) + s->bump;
			for (unsigned i = k; i-- > 0; ) {
				void *o = base + i * cs;
				*(void **)o = link_encode(o, tb->head);
				tb->head = o;
			}
			s->bump += (uint32_t)(k * cs);
			s->used = (uint16_t)(s->used + k);
			got += k;
			tb->count += k;
		}
		while (got < want && s->free) {
			void *o = s->free;
			s->free = link_decode(o);
			*(void **)o = link_encode(o, tb->head);
			tb->head = o;
			tb->count++;
			s->used++;
			got++;
		}
		if (s->used == s->capacity) partial_remove(c, s);
	}
	__unlock(&c->lock);
	return got;
}

/* Return n objects from the top of a tcache bin to their spans. */
static void central_flush(unsigned cls, struct tcache_bin *tb, unsigned n)
{
	struct class_state *c = &classes[cls];
	size_t cs = __malloc_class_size[cls];

	__lock(&c->lock);
	while (n-- && tb->head) {
		void *o = tb->head;
		tb->head = link_decode(o);
		tb->count--;
		struct segment *g = seg_of(o);
		struct span *s = span_of(g, o);
		size_t off = (size_t)((unsigned char *)o - span_base(s));
		if (unlikely(s->kind != SPAN_SMALL || s->cls != cls || off % cs || !s->used))
			malloc_abort("free(): invalid pointer");
		*(void **)o = link_encode(o, s->free);
		s->free = o;
		if (s->used == s->capacity) partial_insert(c, s);
		s->used--;
		if (!s->used && (c->partial != s || s->next)) {
			/* Fully free and not the class's last span: back to the heap. */
			partial_remove(c, s);
			__lock(&heap_lock);
			heap_free(s, 1);
			__unlock(&heap_lock);
		}
	}
	__unlock(&c->lock);
}

/* ---------------------------------------------------------------------- */
/* thread caches                                                           */
/* ---------------------------------------------------------------------- */

static void tcache_init_bins(struct malloc_tcache *tc)
{
	for (unsigned i = 0; i < NCLASS; i++) {
		uint32_t m = 16384 / __malloc_class_size[i];
		tc->bin[i].head = 0;
		tc->bin[i].count = 0;
		tc->bin[i].max = m < 2 ? 2 : m > 128 ? 128 : m;
	}
}

static cold struct malloc_tcache *tcache_create(struct pthread *self)
{
	struct malloc_tcache *tc;
	if (self->is_main) {
		tc = &main_tcache;
	} else {
		/* Allocate the cache itself straight from the central class. */
		struct tcache_bin tmp = { 0, 0, 1 };
		unsigned cls = size_to_class(sizeof *tc);
		if (!central_fill(cls, &tmp, 1)) return 0;
		tc = tmp.head;
		/* tmp.head held exactly one object; its link is not needed */
	}
	tcache_init_bins(tc);
	self->tcache = tc;
	return tc;
}

static __inline struct malloc_tcache *get_tcache(void)
{
	struct pthread *self = __self();
	struct malloc_tcache *tc = self->tcache;
	if (unlikely(!tc)) tc = tcache_create(self);
	return tc;
}

hidden void __malloc_thread_exit(struct pthread *self)
{
	struct malloc_tcache *tc = self->tcache;
	if (!tc) return;
	for (unsigned i = 0; i < NCLASS; i++)
		if (tc->bin[i].count) central_flush(i, &tc->bin[i], tc->bin[i].count);
	self->tcache = 0;
	if (tc != &main_tcache) {
		struct tcache_bin tmp = { 0, 0, 1 };
		*(void **)tc = link_encode(tc, 0);
		tmp.head = tc;
		tmp.count = 1;
		central_flush(size_to_class(sizeof *tc), &tmp, 1);
	}
}

/* ---------------------------------------------------------------------- */
/* small objects                                                           */
/* ---------------------------------------------------------------------- */

/* Objects sitting in a thread cache carry the cache's key in their second
 * word; a free of an object that still carries it is checked against the
 * bin (glibc-style double-free detection).  Allocation clears the tag. */
static __inline uintptr_t tc_key(const struct malloc_tcache *tc)
{
	return (uintptr_t)tc ^ __libc.secret;
}

static __inline void *tc_pop(struct tcache_bin *b)
{
	void *o = b->head;
	if (likely(o)) {
		b->head = link_decode(o);
		b->count--;
		((uintptr_t *)o)[1] = 0;
	}
	return o;
}

static noinline void *small_slow(unsigned cls)
{
	struct malloc_tcache *tc = get_tcache();
	if (!tc) return 0;
	struct tcache_bin *b = &tc->bin[cls];
	unsigned want = b->max / 2 ? b->max / 2 : 1;
	if (!central_fill(cls, b, want)) return 0;
	return tc_pop(b);
}

static __inline void *small_alloc(unsigned cls)
{
	struct malloc_tcache *tc = __self()->tcache;
	if (likely(tc != 0)) {
		void *o = tc_pop(&tc->bin[cls]);
		if (likely(o != 0)) return o;
	}
	return small_slow(cls);
}

static cold void tc_check_double(const struct tcache_bin *b, void *p)
{
	unsigned n = 0;
	for (void *o = b->head; o && n <= b->count; o = link_decode(o), n++)
		if (o == p) malloc_abort("free(): double free detected");
}

static void small_free(void *p, struct span *s)
{
	unsigned cls = s->cls;
	size_t off = (size_t)((unsigned char *)p - span_base(s));
	size_t q = (size_t)((off * __malloc_class_magic[cls]) >> 40);
	if (unlikely(q * __malloc_class_size[cls] != off || q >= s->capacity))
		malloc_abort("free(): invalid pointer");
	struct malloc_tcache *tc = get_tcache();
	if (unlikely(!tc)) {
		struct tcache_bin tmp = { 0, 0, 1 };
		*(void **)p = link_encode(p, 0);
		tmp.head = p;
		tmp.count = 1;
		central_flush(cls, &tmp, 1);
		return;
	}
	struct tcache_bin *b = &tc->bin[cls];
	uintptr_t key = tc_key(tc);
	if (unlikely(((uintptr_t *)p)[1] == key)) tc_check_double(b, p);
	((uintptr_t *)p)[1] = key;
	*(void **)p = link_encode(p, b->head);
	b->head = p;
	if (unlikely(++b->count > b->max)) central_flush(cls, b, b->count / 2);
}

/* ---------------------------------------------------------------------- */
/* large spans                                                             */
/* ---------------------------------------------------------------------- */

static void *large_alloc(size_t n, int *zeroed)
{
	size_t pages = (n + PAGE_SZ - 1) >> PAGE_SHIFT;
	__lock(&heap_lock);
	struct span *s = heap_alloc(pages);
	if (s) {
		if (zeroed) *zeroed = !s->dirty;
		s->kind = SPAN_LARGE;
		map_span(s, 0);
	}
	__unlock(&heap_lock);
	return s ? span_base(s) : 0;
}

/* Large span whose start is aligned to `align` (a power of two > PAGE_SZ). */
static void *large_alloc_aligned(size_t n, size_t align)
{
	size_t pages = (n + PAGE_SZ - 1) >> PAGE_SHIFT;
	size_t extra = (align >> PAGE_SHIFT) - 1;
	__lock(&heap_lock);
	struct span *s = heap_alloc(pages + extra);
	if (!s) {
		__unlock(&heap_lock);
		return 0;
	}
	uintptr_t base = (uintptr_t)span_base(s);
	size_t lead = ((((base + align - 1) & -(uintptr_t)align) - base) >> PAGE_SHIFT);
	int dirty = s->dirty;
	if (lead) {
		/* the leading pages become a free span of their own */
		struct segment *g = seg_of(s);
		struct span *t = &g->spans[s->start + lead];
		t->start = (uint16_t)(s->start + lead);
		t->npages = (uint16_t)(s->npages - lead);
		t->dirty = (uint8_t)dirty;
		t->in_list = 0;
		s->npages = (uint16_t)lead;
		map_span(s, 0);
		heap_free(s, dirty);
		s = t;
	}
	if (s->npages > pages) split_tail(s, pages, dirty);
	s->kind = SPAN_LARGE;
	map_span(s, 0);
	__unlock(&heap_lock);
	return span_base(s);
}

/* ---------------------------------------------------------------------- */
/* huge mappings                                                           */
/* ---------------------------------------------------------------------- */

static __inline struct segment *hdr_of(const void *p)
{
	uintptr_t a = (uintptr_t)p;
	return (struct segment *)((a & (SEG_SIZE - 1)) ? (a & ~(SEG_SIZE - 1)) : a - PAGE_SZ);
}

static void huge_layout(size_t align, size_t *off, size_t *map_align, size_t *phase)
{
	if (align >= SEG_SIZE) {
		*off = PAGE_SZ;
		*map_align = align;
		*phase = PAGE_SZ;
	} else {
		*off = align > PAGE_SZ ? align : PAGE_SZ;
		*map_align = SEG_SIZE;
		*phase = 0;
	}
}

static void *huge_alloc(size_t n, size_t align)
{
	size_t off, malign, phase;
	huge_layout(align, &off, &malign, &phase);
	if (n > SIZE_MAX / 2 - off - malign) return 0;
	size_t len = (off + n + PAGE_SZ - 1) & ~(PAGE_SZ - 1);
	struct segment *g = map_aligned(len, malign, phase);
	if (!g) return 0;
	g->magic = seg_magic();
	g->kind = SEG_HUGE;
	g->map_len = len;
	g->off = off;
	g->hdr_pages = (uint32_t)(align >> PAGE_SHIFT);
	return (unsigned char *)g + off;
}

static void huge_free(struct segment *g)
{
	g->magic = 0;
	sys_unmap(g, g->map_len);
}

static void *huge_realloc(struct segment *g, size_t n)
{
	size_t off = g->off;
	if (n > SIZE_MAX / 2 - off) return 0;
	size_t len = (off + n + PAGE_SZ - 1) & ~(PAGE_SZ - 1);
	if (len <= g->map_len) {
		sys_unmap((unsigned char *)g + len, g->map_len - len);
		g->map_len = len;
		return (unsigned char *)g + off;
	}
	long r = __syscall(SYS_mremap, g, g->map_len, len, 0, 0);
	if (!__is_err(r)) {
		g->map_len = len;
		return (unsigned char *)g + off;
	}
	/* Move the pages (header included) to a correctly aligned new place. */
	size_t align = (size_t)g->hdr_pages << PAGE_SHIFT, o2, malign, phase;
	huge_layout(align, &o2, &malign, &phase);
	void *dst = map_aligned(len, malign, phase);
	if (!dst) return 0;
	r = __syscall(SYS_mremap, g, g->map_len, len, MREMAP_MAYMOVE | MREMAP_FIXED, dst);
	if (__is_err(r)) {
		sys_unmap(dst, len);
		return 0;
	}
	g = dst;
	g->map_len = len;
	return (unsigned char *)g + off;
}

/* ---------------------------------------------------------------------- */
/* pointer validation                                                      */
/* ---------------------------------------------------------------------- */

static __inline struct segment *checked_hdr(const void *p)
{
	struct segment *g = hdr_of(p);
	if (unlikely(g->magic != seg_magic())) malloc_abort("free(): invalid pointer");
	if (unlikely(g->kind == SEG_HUGE && (unsigned char *)g + g->off != (const unsigned char *)p))
		malloc_abort("free(): invalid pointer");
	return g;
}

/* ---------------------------------------------------------------------- */
/* public interface                                                        */
/* ---------------------------------------------------------------------- */

static void *alloc_any(size_t n, int *zeroed)
{
	if (zeroed) *zeroed = 0;
	if (likely(n <= SMALL_MAX)) return small_alloc(size_to_class(n));
	if (n <= HUGE_THRESHOLD) return large_alloc(n, zeroed);
	if (n > PTRDIFF_MAX) return 0;
	if (zeroed) *zeroed = 1;
	return huge_alloc(n, 0);
}

void *malloc(size_t n)
{
	void *p = alloc_any(n, 0);
	if (unlikely(!p)) errno = ENOMEM;
	return p;
}

void *calloc(size_t m, size_t n)
{
	size_t total;
	int zeroed;
	if (__builtin_mul_overflow(m, n, &total)) {
		errno = ENOMEM;
		return 0;
	}
	void *p = alloc_any(total, &zeroed);
	if (unlikely(!p)) {
		errno = ENOMEM;
		return 0;
	}
	if (!zeroed) memset(p, 0, total);
	return p;
}

void free(void *p)
{
	if (!p) return;
	if (unlikely(((uintptr_t)p & 15) != 0)) malloc_abort("free(): invalid pointer");
	struct segment *g = checked_hdr(p);
	if (unlikely(g->kind != SEG_NORMAL)) {
		huge_free(g);
		return;
	}
	struct span *s = span_of(g, p);
	if (likely(s->kind == SPAN_SMALL)) {
		small_free(p, s);
		return;
	}
	if (s->kind == SPAN_LARGE && (unsigned char *)p == span_base(s)) {
		__lock(&heap_lock);
		heap_free(s, 1);
		__unlock(&heap_lock);
		return;
	}
	malloc_abort(s->kind == SPAN_FREE ? "free(): double free detected" : "free(): invalid pointer");
}

size_t malloc_usable_size(void *p)
{
	if (!p) return 0;
	struct segment *g = checked_hdr(p);
	if (g->kind != SEG_NORMAL) return g->map_len - g->off;
	struct span *s = span_of(g, p);
	if (s->kind == SPAN_SMALL) return __malloc_class_size[s->cls];
	if (s->kind == SPAN_LARGE) return (size_t)s->npages << PAGE_SHIFT;
	malloc_abort("malloc_usable_size(): invalid pointer");
}

static void *realloc_move(void *p, size_t old, size_t n)
{
	void *q = malloc(n);
	if (!q) return 0;
	memcpy(q, p, old < n ? old : n);
	free(p);
	return q;
}

static int large_resize(struct span *s, size_t n)
{
	size_t pages = (n + PAGE_SZ - 1) >> PAGE_SHIFT;
	int ok = 0;
	__lock(&heap_lock);
	if (pages == s->npages) {
		ok = 1;
	} else if (pages < s->npages) {
		struct segment *g = seg_of(s);
		struct span *t = &g->spans[s->start + pages];
		t->start = (uint16_t)(s->start + pages);
		t->npages = (uint16_t)(s->npages - pages);
		t->in_list = 0;
		s->npages = (uint16_t)pages;
		map_span(s, 0);
		heap_free(t, 1);
		ok = 1;
	} else {
		struct segment *g = seg_of(s);
		unsigned end = s->start + s->npages;
		if (end < SEG_PAGES) {
			struct span *nx = &g->spans[end];
			if (nx->kind == SPAN_FREE && nx->in_list && s->npages + nx->npages >= pages) {
				bin_remove(nx);
				size_t need = pages - s->npages;
				int dirty = nx->dirty;
				if (nx->npages > need) split_tail(nx, need, dirty);
				s->npages = (uint16_t)pages;
				map_span(s, 0);
				ok = 1;
			}
		}
	}
	__unlock(&heap_lock);
	return ok;
}

void *realloc(void *p, size_t n)
{
	if (!p) return malloc(n);
	if (!n) {
		free(p);
		return 0;
	}
	if (unlikely(((uintptr_t)p & 15) != 0)) malloc_abort("realloc(): invalid pointer");
	if (n > PTRDIFF_MAX) {
		errno = ENOMEM;
		return 0;
	}
	struct segment *g = checked_hdr(p);
	if (g->kind != SEG_NORMAL) {
		size_t old = g->map_len - g->off;
		if (n > HUGE_THRESHOLD / 2 || g->hdr_pages) {
			void *q = huge_realloc(g, n);
			if (!q) errno = ENOMEM;
			return q;
		}
		return realloc_move(p, old, n);
	}
	struct span *s = span_of(g, p);
	if (s->kind == SPAN_SMALL) {
		unsigned cls = s->cls, nc = n <= SMALL_MAX ? size_to_class(n) : NCLASS;
		size_t cs = __malloc_class_size[cls];
		if (nc == cls || (nc < cls && n > cs / 2)) return p;
		return realloc_move(p, cs, n);
	}
	if (s->kind != SPAN_LARGE || (unsigned char *)p != span_base(s))
		malloc_abort("realloc(): invalid pointer");
	size_t old = (size_t)s->npages << PAGE_SHIFT;
	if (n > SMALL_MAX && n <= HUGE_THRESHOLD && large_resize(s, n)) return p;
	return realloc_move(p, old, n);
}

void *reallocarray(void *p, size_t m, size_t n)
{
	size_t total;
	if (__builtin_mul_overflow(m, n, &total)) {
		errno = ENOMEM;
		return 0;
	}
	return realloc(p, total);
}

static void *alloc_aligned(size_t align, size_t n)
{
	if (align <= 16) return malloc(n);
	if (n > PTRDIFF_MAX) return 0;
	if (n <= SMALL_MAX && align <= PAGE_SZ) {
		for (unsigned c = size_to_class(n); c < NCLASS; c++)
			if (__malloc_class_size[c] % align == 0) return small_alloc(c);
	}
	if (align < PAGE_SZ) align = PAGE_SZ;
	if (!n) n = 1;
	if (n <= HUGE_THRESHOLD && align <= HUGE_THRESHOLD &&
	    ((n + PAGE_SZ - 1) >> PAGE_SHIFT) + (align >> PAGE_SHIFT) <= SEG_USABLE) {
		if (align == PAGE_SZ) return large_alloc(n, 0);
		return large_alloc_aligned(n, align);
	}
	return huge_alloc(n, align);
}

int posix_memalign(void **res, size_t align, size_t n)
{
	if (align < sizeof(void *) || (align & (align - 1))) return EINVAL;
	void *p = alloc_aligned(align, n);
	if (!p) return ENOMEM;
	*res = p;
	return 0;
}

void *aligned_alloc(size_t align, size_t n)
{
	if (!align || (align & (align - 1))) {
		errno = EINVAL;
		return 0;
	}
	void *p = alloc_aligned(align, n);
	if (!p) errno = ENOMEM;
	return p;
}

void *memalign(size_t align, size_t n)
{
	return aligned_alloc(align, n);
}

void *valloc(size_t n)
{
	return aligned_alloc(PAGE_SZ, n);
}

void *pvalloc(size_t n)
{
	if (n > SIZE_MAX - PAGE_SZ) {
		errno = ENOMEM;
		return 0;
	}
	return aligned_alloc(PAGE_SZ, (n + PAGE_SZ - 1) & ~(PAGE_SZ - 1));
}

int malloc_trim(size_t pad)
{
	__lock(&heap_lock);
	size_t before = dirty_free_pages;
	purge(pad >> PAGE_SHIFT);
	int r = dirty_free_pages != before;
	__unlock(&heap_lock);
	return r;
}

/* fork(): who < 0 before, 0 in the parent after, > 0 in the child after. */
hidden void __malloc_atfork(int who)
{
	if (who < 0) {
		for (unsigned i = 0; i < NCLASS; i++) __lock(&classes[i].lock);
		__lock(&heap_lock);
	} else if (who == 0) {
		__unlock(&heap_lock);
		for (unsigned i = NCLASS; i-- > 0; ) __unlock(&classes[i].lock);
	} else {
		heap_lock = 0;
		for (unsigned i = 0; i < NCLASS; i++) classes[i].lock = 0;
	}
}
