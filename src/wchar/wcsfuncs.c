/* lib-spfxd — wide-character string and memory functions. */
#include <wchar.h>
#include <wctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "libc.h"

size_t wcslen(const wchar_t *s)
{
	const wchar_t *a = s;
	while (*s) s++;
	return (size_t)(s - a);
}

size_t wcsnlen(const wchar_t *s, size_t n)
{
	const wchar_t *z = wmemchr(s, 0, n);
	return z ? (size_t)(z - s) : n;
}

wchar_t *wcpcpy(wchar_t *restrict d, const wchar_t *restrict s)
{
	while ((*d = *s)) d++, s++;
	return d;
}

wchar_t *wcscpy(wchar_t *restrict d, const wchar_t *restrict s)
{
	wcpcpy(d, s);
	return d;
}

wchar_t *wcpncpy(wchar_t *restrict d, const wchar_t *restrict s, size_t n)
{
	size_t l = wcsnlen(s, n);
	wmemcpy(d, s, l);
	wmemset(d + l, 0, n - l);
	return d + l;
}

wchar_t *wcsncpy(wchar_t *restrict d, const wchar_t *restrict s, size_t n)
{
	wcpncpy(d, s, n);
	return d;
}

wchar_t *wcscat(wchar_t *restrict d, const wchar_t *restrict s)
{
	wcscpy(d + wcslen(d), s);
	return d;
}

wchar_t *wcsncat(wchar_t *restrict d, const wchar_t *restrict s, size_t n)
{
	wchar_t *e = d + wcslen(d);
	size_t l = wcsnlen(s, n);
	wmemcpy(e, s, l);
	e[l] = 0;
	return d;
}

/* wchar_t is signed; ordering compares the values as signed integers. */
int wcscmp(const wchar_t *l, const wchar_t *r)
{
	for (; *l == *r && *l; l++, r++);
	return *l < *r ? -1 : *l > *r;
}

int wcsncmp(const wchar_t *l, const wchar_t *r, size_t n)
{
	for (; n && *l == *r && *l; n--, l++, r++);
	return n ? (*l < *r ? -1 : *l > *r) : 0;
}

int wcscoll(const wchar_t *l, const wchar_t *r)
{
	return wcscmp(l, r);
}

size_t wcsxfrm(wchar_t *restrict d, const wchar_t *restrict s, size_t n)
{
	size_t l = wcslen(s);
	if (l < n) wmemcpy(d, s, l + 1);
	return l;
}

wchar_t *wcschrnul(const wchar_t *s, wchar_t c)
{
	while (*s && *s != c) s++;
	return (wchar_t *)s;
}

wchar_t *wcschr(const wchar_t *s, wchar_t c)
{
	if (!c) return (wchar_t *)s + wcslen(s);
	s = wcschrnul(s, c);
	return *s ? (wchar_t *)s : 0;
}

wchar_t *wcsrchr(const wchar_t *s, wchar_t c)
{
	const wchar_t *p = s + wcslen(s);
	for (;; p--) {
		if (*p == c) return (wchar_t *)p;
		if (p == s) return 0;
	}
}

size_t wcsspn(const wchar_t *s, const wchar_t *acc)
{
	const wchar_t *a = s;
	for (; *s && wcschr(acc, *s); s++);
	return (size_t)(s - a);
}

size_t wcscspn(const wchar_t *s, const wchar_t *rej)
{
	const wchar_t *a = s;
	for (; *s && !wcschr(rej, *s); s++);
	return (size_t)(s - a);
}

wchar_t *wcspbrk(const wchar_t *s, const wchar_t *b)
{
	s += wcscspn(s, b);
	return *s ? (wchar_t *)s : 0;
}

wchar_t *wcstok(wchar_t *restrict s, const wchar_t *restrict sep, wchar_t **restrict p)
{
	if (!s && !(s = *p)) return 0;
	s += wcsspn(s, sep);
	if (!*s) return *p = 0;
	*p = s + wcscspn(s, sep);
	if (**p) *(*p)++ = 0;
	else *p = 0;
	return s;
}

/* Wide substring search: the needle is short in practice; use a
 * first-character scan with full comparison (O(n*m) worst case is bounded
 * by switching to a window hash beyond a threshold). */
wchar_t *wcsstr(const wchar_t *restrict h, const wchar_t *restrict n)
{
	size_t nl = wcslen(n);
	if (!nl) return (wchar_t *)h;
	if (nl == 1) return wcschr(h, n[0]);
	/* Rabin-Karp with a 64-bit rolling hash: linear expected time */
	const uint64_t B = 0x100000001b3ULL;
	uint64_t hn = 0, hh = 0, pw = 1;
	size_t i;
	for (i = 0; i < nl; i++) {
		if (!h[i]) return 0;
		hn = hn * B + (uint32_t)n[i];
		hh = hh * B + (uint32_t)h[i];
		if (i) pw *= B;
	}
	for (i = 0;; i++) {
		if (hh == hn && !wmemcmp(h + i, n, nl)) return (wchar_t *)h + i;
		if (!h[i + nl]) return 0;
		hh = (hh - (uint32_t)h[i] * pw) * B + (uint32_t)h[i + nl];
	}
}

wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n)
{
	for (; n; n--, s++)
		if (*s == c) return (wchar_t *)s;
	return 0;
}

int wmemcmp(const wchar_t *l, const wchar_t *r, size_t n)
{
	for (; n && *l == *r; n--, l++, r++);
	return n ? (*l < *r ? -1 : *l > *r) : 0;
}

wchar_t *wmemcpy(wchar_t *restrict d, const wchar_t *restrict s, size_t n)
{
	return memcpy(d, s, n * sizeof(wchar_t));
}

wchar_t *wmempcpy(wchar_t *restrict d, const wchar_t *restrict s, size_t n)
{
	return (wchar_t *)memcpy(d, s, n * sizeof(wchar_t)) + n;
}

wchar_t *wmemmove(wchar_t *d, const wchar_t *s, size_t n)
{
	return memmove(d, s, n * sizeof(wchar_t));
}

wchar_t *wmemset(wchar_t *d, wchar_t c, size_t n)
{
	for (size_t i = 0; i < n; i++) d[i] = c;
	return d;
}

wchar_t *wcsdup(const wchar_t *s)
{
	size_t l = wcslen(s) + 1;
	wchar_t *d = malloc(l * sizeof *d);
	return d ? wmemcpy(d, s, l) : 0;
}

int wcscasecmp(const wchar_t *l, const wchar_t *r)
{
	for (; *l && (*l == *r || towlower((wint_t)*l) == towlower((wint_t)*r)); l++, r++);
	wint_t a = towlower((wint_t)*l), b = towlower((wint_t)*r);
	return a < b ? -1 : a > b;
}

int wcsncasecmp(const wchar_t *l, const wchar_t *r, size_t n)
{
	for (; n && *l && (*l == *r || towlower((wint_t)*l) == towlower((wint_t)*r)); n--, l++, r++);
	if (!n) return 0;
	wint_t a = towlower((wint_t)*l), b = towlower((wint_t)*r);
	return a < b ? -1 : a > b;
}

int wcwidth(wchar_t c)
{
	if ((uint32_t)c > 0x10ffff) return -1;
	return __uni_width((uint32_t)c);
}

int wcswidth(const wchar_t *s, size_t n)
{
	int total = 0;
	for (; n && *s; n--, s++) {
		int w = wcwidth(*s);
		if (w < 0) return -1;
		total += w;
	}
	return total;
}
