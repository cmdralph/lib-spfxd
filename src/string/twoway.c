/*
 * lib-spfxd — substring search core shared by strstr, memmem and strcasestr.
 *
 * Needles of up to 8 bytes are matched with a sliding 64-bit window: every
 * haystack byte is shifted in once and the window compared against the
 * packed needle, which is linear time with a tiny constant.
 *
 * Longer needles use the Two-Way algorithm (Crochemore & Perrin, 1991):
 * the needle is split at a critical factorization x = u.v computed from the
 * maximal suffixes under both lexicographic orders.  v is matched left to
 * right; on success u is matched right to left.  Mismatches shift by a
 * safe amount derived from the period, and for periodic needles the
 * already-matched prefix is remembered.  Worst case O(n + m) time, O(1)
 * extra space, no allocation.
 *
 * For NUL-terminated haystacks (strstr) the haystack length is discovered
 * lazily with strnlen, so a match near the start does not scan the whole
 * haystack first.
 */
#include <string.h>
#include "string_impl.h"

static __inline unsigned canon(unsigned c, int fold)
{
	return (fold && c - 'A' < 26u) ? (c | 0x20) : c;
}

static size_t max_suffix(const unsigned char *x, size_t m, size_t *period, int rev, int fold)
{
	size_t ms = (size_t)-1, j = 0, k = 1, p = 1;
	while (j + k < m) {
		unsigned a = canon(x[j + k], fold), b = canon(x[ms + k], fold);
		if (rev ? a > b : a < b) {
			j += k;
			k = 1;
			p = j - ms;
		} else if (a == b) {
			if (k != p) k++;
			else { j += p; k = 1; }
		} else {
			ms = j;
			j = ms + 1;
			k = p = 1;
		}
	}
	*period = p;
	return ms;
}

/* Make at least `need` bytes of the haystack available.  In string mode
 * this grows `*avail` up to the terminator; returns 0 if impossible. */
static __inline int ensure(const unsigned char *h, size_t *avail, size_t need, int strmode)
{
	if (*avail >= need) return 1;
	if (!strmode) return 0;
	size_t want = need - *avail;
	if (want < 4096) want = 4096;
	size_t got = strnlen((const char *)h + *avail, want);
	*avail += got;
	return *avail >= need;
}

static unsigned char *short_search(const unsigned char *h, size_t avail, int strmode,
	const unsigned char *n, size_t m, int fold)
{
	uint64_t mask = m == 8 ? ~0ULL : (1ULL << (8 * m)) - 1, nw = 0, hw = 0;
	size_t i;
	for (i = 0; i < m; i++) nw = nw << 8 | canon(n[i], fold);
	for (i = 0; ; i++) {
		unsigned c;
		if (strmode) {
			c = h[i];
			if (!c) return 0;
		} else {
			if (i >= avail) return 0;
			c = h[i];
		}
		hw = (hw << 8 | canon(c, fold)) & mask;
		if (i + 1 >= m && hw == nw) return (unsigned char *)h + i + 1 - m;
	}
}

hidden char *__substr_search(const unsigned char *h, size_t avail, int strmode,
	const unsigned char *n, size_t m, int fold)
{
	size_t p1, p2, p, ell, ms1, ms2;
	long i, mem, ellv;
	size_t j;

	if (!m) return (char *)h;
	if (m <= 8) return (char *)short_search(h, avail, strmode, n, m, fold);
	if (!ensure(h, &avail, m, strmode)) return 0;

	ms1 = max_suffix(n, m, &p1, 0, fold);
	ms2 = max_suffix(n, m, &p2, 1, fold);
	if (ms1 + 1 > ms2 + 1) { ell = ms1; p = p1; }
	else { ell = ms2; p = p2; }
	ellv = (long)(ell + 1) - 1;    /* ell as signed (may be -1) */

	int periodic = 1;
	for (size_t k = 0; k < ell + 1; k++)
		if (canon(n[k], fold) != canon(n[k + p], fold)) { periodic = 0; break; }

	if (periodic) {
		mem = -1;
		for (j = 0; ensure(h, &avail, j + m, strmode); ) {
			i = (ellv > mem ? ellv : mem) + 1;
			while ((size_t)i < m && canon(n[i], fold) == canon(h[i + j], fold)) i++;
			if ((size_t)i >= m) {
				i = ellv;
				while (i > mem && canon(n[i], fold) == canon(h[i + j], fold)) i--;
				if (i <= mem) return (char *)h + j;
				j += p;
				mem = (long)(m - p) - 1;
			} else {
				j += (size_t)(i - ellv);
				mem = -1;
			}
		}
	} else {
		size_t a = ell + 1, b = m - ell - 1;
		p = (a > b ? a : b) + 1;
		for (j = 0; ensure(h, &avail, j + m, strmode); ) {
			i = ellv + 1;
			while ((size_t)i < m && canon(n[i], fold) == canon(h[i + j], fold)) i++;
			if ((size_t)i >= m) {
				i = ellv;
				while (i >= 0 && canon(n[i], fold) == canon(h[i + j], fold)) i--;
				if (i < 0) return (char *)h + j;
				j += p;
			} else {
				j += (size_t)(i - ellv);
			}
		}
	}
	return 0;
}
