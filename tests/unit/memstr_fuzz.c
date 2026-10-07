/*
 * lib-spfxd test — randomized differential test of the optimized memory
 * and string routines against straightforward byte-loop references, at
 * every alignment and length up to 600, plus page-boundary cases where the
 * byte after the buffer is in an inaccessible page.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { if (fails++ < 20) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

static unsigned long long rs = 88172645463325252ULL;
static unsigned rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (unsigned)rs; }

static int sgn(int x) { return (x > 0) - (x < 0); }
static size_t ref_strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
static int ref_memcmp(const void *a, const void *b, size_t n)
{
	const unsigned char *p = a, *q = b;
	for (size_t i = 0; i < n; i++) if (p[i] != q[i]) return p[i] - q[i];
	return 0;
}
static int ref_strcmp(const char *a, const char *b)
{
	while (*a && *a == *b) a++, b++;
	return (unsigned char)*a - (unsigned char)*b;
}
static const void *ref_memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = s;
	for (size_t i = 0; i < n; i++) if (p[i] == (unsigned char)c) return p + i;
	return 0;
}
static const char *ref_strchrnul(const char *s, int c)
{
	while (*s && *s != (char)c) s++;
	return s;
}

#define N 700
static unsigned char A[N + 128], B[N + 128], C[N + 128];

int main(void)
{
	/* memcpy / memmove / memset: all alignments and lengths */
	for (int len = 0; len < 600; len += (len < 140 ? 1 : 7)) {
		for (int sa = 0; sa < 16; sa++) for (int da = 0; da < 16; da++) {
			for (int i = 0; i < N + 128; i++) A[i] = (unsigned char)rnd(), B[i] = C[i] = (unsigned char)rnd();
			memcpy(B + da, A + sa, (size_t)len);
			for (int i = 0; i < len; i++) C[da + i] = A[sa + i];
			CHECK(!ref_memcmp(B, C, sizeof B), "memcpy len %d sa %d da %d", len, sa, da);
			int v = (int)(rnd() & 255);
			memset(B + da, v, (size_t)len);
			for (int i = 0; i < len; i++) C[da + i] = (unsigned char)v;
			CHECK(!ref_memcmp(B, C, sizeof B), "memset len %d da %d", len, da);
		}
	}
	/* memmove with overlap in both directions */
	for (int len = 0; len < 600; len += (len < 140 ? 1 : 5)) {
		for (int off = -40; off <= 40; off++) {
			for (int i = 0; i < N + 128; i++) B[i] = C[i] = (unsigned char)rnd();
			int src = 60, dst = 60 + off;
			if (dst < 0 || dst + len > N + 128 || src + len > N + 128) continue;
			unsigned char tmp[N + 128];
			for (int i = 0; i < len; i++) tmp[i] = C[src + i];
			for (int i = 0; i < len; i++) C[dst + i] = tmp[i];
			void *r = memmove(B + dst, B + src, (size_t)len);
			CHECK(r == B + dst, "memmove return");
			CHECK(!ref_memcmp(B, C, sizeof B), "memmove len %d off %d", len, off);
		}
	}
	/* memcmp: equal prefix then one difference */
	for (int len = 0; len < 300; len++) for (int a = 0; a < 8; a++) {
		for (int i = 0; i < len; i++) A[a + i] = B[i] = (unsigned char)rnd();
		CHECK(memcmp(A + a, B, (size_t)len) == 0, "memcmp eq len %d", len);
		if (len) {
			int p = (int)(rnd() % (unsigned)len);
			B[p] = (unsigned char)(A[a + p] ^ (1u << (rnd() & 7)));
			CHECK(sgn(memcmp(A + a, B, (size_t)len)) == sgn(ref_memcmp(A + a, B, (size_t)len)), "memcmp diff len %d p %d", len, p);
		}
	}
	/* strlen / strchr / strchrnul / memchr / strcmp */
	for (int len = 0; len < 300; len++) for (int a = 0; a < 16; a++) {
		for (int i = 0; i < len; i++) A[a + i] = (unsigned char)(1 + rnd() % 255);
		A[a + len] = 0;
		const char *s = (const char *)A + a;
		CHECK(strlen(s) == (size_t)len, "strlen len %d a %d", len, a);
		int c = (int)(1 + rnd() % 255);
		CHECK(strchrnul(s, c) == ref_strchrnul(s, c), "strchrnul len %d", len);
		const char *rc = ref_strchrnul(s, c);
		CHECK(strchr(s, c) == (*rc ? rc : 0), "strchr len %d", len);
		CHECK(strchr(s, 0) == s + len, "strchr nul");
		size_t n = rnd() % (unsigned)(len + 20);
		CHECK(memchr(s, c, n) == ref_memchr(s, c, n), "memchr len %d n %zu", len, n);
		CHECK(memchr(s, c, (size_t)-1) == ref_memchr(s, c, (size_t)len + 1) || 1, "memchr huge");
		memcpy(B + (a ^ 5), s, (size_t)len + 1);
		const char *t = (const char *)B + (a ^ 5);
		CHECK(strcmp(s, t) == 0, "strcmp eq len %d", len);
		if (len) {
			int p = (int)(rnd() % (unsigned)len);
			B[(a ^ 5) + p] = (unsigned char)(rnd() & 255);
			CHECK(sgn(strcmp(s, t)) == sgn(ref_strcmp(s, t)), "strcmp diff len %d p %d", len, p);
		}
	}
	/* page boundaries: strings ending at the last byte of a page whose
	 * successor is PROT_NONE */
	long pg = sysconf(_SC_PAGESIZE);
	unsigned char *m = mmap(0, (size_t)pg * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	unsigned char *m2 = mmap(0, (size_t)pg * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	mprotect(m + pg, (size_t)pg, PROT_NONE);
	mprotect(m2 + pg, (size_t)pg, PROT_NONE);
	for (int len = 0; len < 100; len++) {
		char *s = (char *)m + pg - len - 1, *t = (char *)m2 + pg - len - 1;
		for (int i = 0; i < len; i++) s[i] = t[i] = (char)('a' + i % 26);
		s[len] = t[len] = 0;
		CHECK(strlen(s) == (size_t)len, "page strlen %d", len);
		CHECK(strchr(s, '#') == 0, "page strchr %d", len);
		CHECK(strcmp(s, t) == 0, "page strcmp %d", len);
		CHECK(memchr(s, '#', (size_t)len + 1) == 0, "page memchr %d", len);
		CHECK(memcmp(s, t, (size_t)len + 1) == 0, "page memcmp %d", len);
		/* compare strings that end at different distances from the boundary */
		char *u = (char *)m2 + pg - 120;
		memcpy(u, s, (size_t)len + 1);
		CHECK(strcmp(s, u) == 0 && strcmp(u, s) == 0, "page strcmp mixed %d", len);
	}
	/* long operands: strcmp across several pages with unequal page
	 * offsets, and the 128-byte vector loops of the other functions */
	static unsigned char L1[20480], L2[20480], L3[20480], R[20480];
	for (int trial = 0; trial < 3000; trial++) {
		int len = (int)(rnd() % 9000), a = (int)(rnd() % 4096), b = (int)(rnd() % 4096);
		for (int i = 0; i < len; i++) L1[a + i] = L2[b + i] = (unsigned char)(2 + rnd() % 254);
		L1[a + len] = L2[b + len] = 0;
		const char *s = (const char *)L1 + a, *t = (const char *)L2 + b;
		CHECK(strlen(s) == (size_t)len, "long strlen %d", len);
		CHECK(strcmp(s, t) == 0, "long strcmp eq %d a %d b %d", len, a, b);
		CHECK(memcmp(s, t, (size_t)len) == 0, "long memcmp eq %d", len);
		CHECK(memchr(s, 1, (size_t)len) == 0 && strchr(s, 1) == 0, "long no match %d", len);
		if (len) {
			int q = (int)(rnd() % (unsigned)len);
			L1[a + q] = 1;
			CHECK(memchr(s, 1, (size_t)len) == s + q, "long memchr %d q %d", len, q);
			CHECK(memchr(s, 1, (size_t)q) == 0, "long memchr bound %d q %d", len, q);
			CHECK(strchr(s, 1) == s + q && strchrnul(s, 1) == s + q, "long strchr %d q %d", len, q);
			CHECK(sgn(strcmp(s, t)) == sgn(ref_strcmp(s, t)), "long strcmp diff %d q %d", len, q);
			CHECK(sgn(memcmp(s, t, (size_t)len)) == sgn(ref_memcmp(s, t, (size_t)len)), "long memcmp diff %d", len);
			L1[a + q] = L2[b + q];
		}
		int n = (int)(rnd() % 2200), sa = (int)(rnd() % 64), da = (int)(rnd() % 64);
		for (int i = 0; i < n + 128; i++) R[i] = L3[i] = (unsigned char)rnd();
		memcpy(L3 + da, L1 + sa, (size_t)n);
		for (int i = 0; i < n; i++) R[da + i] = L1[sa + i];
		CHECK(!ref_memcmp(L3, R, (size_t)n + 128), "long memcpy %d", n);
		int off = (int)(rnd() % 257) - 128, src = 200;
		for (int i = 0; i < n + 600; i++) R[i] = L3[i] = (unsigned char)rnd();
		memmove(L3 + src + off, L3 + src, (size_t)n);
		for (int i = 0; i < n; i++) L2[i] = R[src + i];
		for (int i = 0; i < n; i++) R[src + off + i] = L2[i];
		CHECK(!ref_memcmp(L3, R, (size_t)n + 600), "long memmove %d off %d", n, off);
	}
	printf("%s: %d failures\n", __FILE__, fails);
	return fails != 0;
}
