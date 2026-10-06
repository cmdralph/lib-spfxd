/*
 * lib-spfxd — qsort / qsort_r: introsort.
 *
 * Quicksort with median-of-three (Tukey's ninther for large partitions)
 * pivots and a Hoare-style partition that stops on keys equal to the pivot,
 * so runs of equal keys split evenly instead of degrading.  Recursion goes
 * into the smaller side only (O(log n) stack).  If the recursion depth
 * exceeds 2*log2(n) — adversarial "median-of-3 killer" input — the range is
 * finished with heapsort, guaranteeing O(n log n) worst case.  Small ranges
 * use insertion sort.  Element swaps are specialized for 4- and 8-byte
 * aligned elements and done word-wise otherwise.
 */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef int (*cmp_fn)(const void *, const void *, void *);

struct ctx {
	size_t w;
	cmp_fn cmp;
	void *arg;
	int kind;   /* 0 generic, 4: uint32 elements, 8: uint64 elements */
};

static __inline void swap(char *a, char *b, const struct ctx *c)
{
	if (c->kind == 8) {
		uint64_t t = *(uint64_t *)a;
		*(uint64_t *)a = *(uint64_t *)b;
		*(uint64_t *)b = t;
	} else if (c->kind == 4) {
		uint32_t t = *(uint32_t *)a;
		*(uint32_t *)a = *(uint32_t *)b;
		*(uint32_t *)b = t;
	} else {
		size_t n = c->w;
		for (; n >= 8; n -= 8, a += 8, b += 8) {
			uint64_t t;
			memcpy(&t, a, 8);
			memcpy(a, b, 8);
			memcpy(b, &t, 8);
		}
		for (; n; n--, a++, b++) {
			char t = *a;
			*a = *b;
			*b = t;
		}
	}
}

#define AT(i) (base + (i) * c->w)
#define CMP(a, b) c->cmp((a), (b), c->arg)

static void insertion_sort(char *base, size_t n, const struct ctx *c)
{
	for (size_t i = 1; i < n; i++)
		for (size_t j = i; j > 0 && CMP(AT(j - 1), AT(j)) > 0; j--)
			swap(AT(j - 1), AT(j), c);
}

static void sift_down(char *base, size_t root, size_t n, const struct ctx *c)
{
	for (;;) {
		size_t child = 2 * root + 1;
		if (child >= n) return;
		if (child + 1 < n && CMP(AT(child), AT(child + 1)) < 0) child++;
		if (CMP(AT(root), AT(child)) >= 0) return;
		swap(AT(root), AT(child), c);
		root = child;
	}
}

static void heap_sort(char *base, size_t n, const struct ctx *c)
{
	for (size_t i = n / 2; i-- > 0; ) sift_down(base, i, n, c);
	for (size_t end = n; end-- > 1; ) {
		swap(base, AT(end), c);
		sift_down(base, 0, end, c);
	}
}

static char *median3(char *a, char *b, char *d, const struct ctx *c)
{
	if (CMP(a, b) < 0) {
		if (CMP(b, d) < 0) return b;
		return CMP(a, d) < 0 ? d : a;
	}
	if (CMP(a, d) < 0) return a;
	return CMP(b, d) < 0 ? d : b;
}

static void introsort(char *base, size_t n, const struct ctx *c, int depth)
{
	while (n > 12) {
		if (depth-- <= 0) {
			heap_sort(base, n, c);
			return;
		}
		char *lo = base, *mid = AT(n / 2), *hi = AT(n - 1), *pv;
		if (n >= 128) {
			size_t s = n / 8;
			lo = median3(lo, AT(s), AT(2 * s), c);
			mid = median3(AT(n / 2 - s), mid, AT(n / 2 + s), c);
			hi = median3(AT(n - 1 - 2 * s), AT(n - 1 - s), hi, c);
		}
		pv = median3(lo, mid, hi, c);
		swap(base, pv, c);

		/* Hoare partition of [1, n) around base[0]; both scans stop on
		 * equal keys so duplicates spread to both sides. */
		size_t i = 1, j = n - 1;
		for (;;) {
			while (i <= j && CMP(AT(i), base) < 0) i++;
			while (i <= j && CMP(AT(j), base) > 0) j--;
			if (i >= j) break;
			swap(AT(i), AT(j), c);
			i++;
			j--;
		}
		swap(base, AT(j), c);

		/* pivot now at j: recurse into the smaller part */
		size_t left = j, right = n - j - 1;
		if (left < right) {
			introsort(base, left, c, depth);
			base = AT(j + 1);
			n = right;
		} else {
			introsort(AT(j + 1), right, c, depth);
			n = left;
		}
	}
	insertion_sort(base, n, c);
}

void qsort_r(void *base, size_t n, size_t w, cmp_fn cmp, void *arg)
{
	struct ctx c = { w, cmp, arg, 0 };
	if (n < 2 || !w) return;
	if (w == 8 && !((uintptr_t)base & 7)) c.kind = 8;
	else if (w == 4 && !((uintptr_t)base & 3)) c.kind = 4;
	int depth = 2 * (64 - __builtin_clzl(n));
	introsort(base, n, &c, depth);
}

static int call_plain(const void *a, const void *b, void *f)
{
	return ((int (*)(const void *, const void *))f)(a, b);
}

void qsort(void *base, size_t n, size_t w, int (*cmp)(const void *, const void *))
{
	qsort_r(base, n, w, call_plain, (void *)cmp);
}

void *bsearch(const void *key, const void *base, size_t n, size_t w,
	int (*cmp)(const void *, const void *))
{
	const char *b = base;
	while (n) {
		const char *mid = b + (n / 2) * w;
		int r = cmp(key, mid);
		if (!r) return (void *)mid;
		if (r > 0) {
			b = mid + w;
			n -= n / 2 + 1;
		} else {
			n /= 2;
		}
	}
	return 0;
}
