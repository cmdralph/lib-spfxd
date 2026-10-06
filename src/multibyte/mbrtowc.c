/*
 * lib-spfxd — multibyte <-> wide character conversion.
 *
 * UTF-8 (LC_CTYPE *.UTF-8): strict RFC 3629 decoding.  Overlong forms,
 * surrogates (U+D800..U+DFFF), code points above U+10FFFF and stray
 * continuation bytes are rejected with EILSEQ, and malformed sequences are
 * detected at the first byte that proves them invalid, even when the
 * sequence is fed one byte at a time.
 *
 * C/POSIX locale: single-byte; byte b <-> wide character b.
 *
 * mbstate_t: __pending holds the code point bits decoded so far and
 * __count holds (total sequence length << 4) | bytes still needed.
 */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <uchar.h>
#include "locale_impl.h"

static mbstate_t internal_state;

/* Is the partial value acceptable after its second byte? */
static int prefix_ok(unsigned v, unsigned total)
{
	if (total == 3) {
		/* v = cp >> 6: rules out overlongs (< 0x800) and surrogates */
		return v >= 0x20 && (v < 0x360 || v > 0x37f);
	}
	/* total == 4: v = cp >> 12 must be in [0x10, 0x10f] */
	return v >= 0x10 && v <= 0x10f;
}

size_t mbrtowc(wchar_t *restrict wc, const char *restrict src, size_t n, mbstate_t *restrict st)
{
	const unsigned char *s = (const unsigned char *)src;
	wchar_t dummy;
	if (!st) st = &internal_state;
	if (!s) {
		if (st->__count) {
			st->__count = st->__pending = 0;
			errno = EILSEQ;
			return (size_t)-1;
		}
		return 0;
	}
	if (!wc) wc = &dummy;
	if (!n) return (size_t)-2;

	if (!__locale_utf8()) {
		*wc = *s;
		return *s ? 1 : 0;
	}

	unsigned v = st->__pending, need = st->__count & 15, total = st->__count >> 4;
	size_t used = 0;
	if (!need) {
		unsigned c = s[0];
		if (c < 0x80) {
			*wc = (wchar_t)c;
			return c ? 1 : 0;
		}
		if (c < 0xc2 || c > 0xf4) goto ilseq;
		if (c < 0xe0) { total = 2; v = c & 0x1f; }
		else if (c < 0xf0) { total = 3; v = c & 0x0f; }
		else { total = 4; v = c & 0x07; }
		need = total - 1;
		used = 1;
	}
	for (; need && used < n; used++, need--) {
		unsigned c = s[used];
		if ((c & 0xc0) != 0x80) goto ilseq;
		v = v << 6 | (c & 0x3f);
		if (need == total - 1 && total >= 3 && !prefix_ok(v, total)) goto ilseq;
	}
	if (need) {
		st->__pending = v;
		st->__count = total << 4 | need;
		return (size_t)-2;
	}
	st->__pending = st->__count = 0;
	*wc = (wchar_t)v;
	return used;
ilseq:
	st->__pending = st->__count = 0;
	errno = EILSEQ;
	return (size_t)-1;
}

size_t mbrlen(const char *restrict s, size_t n, mbstate_t *restrict st)
{
	static mbstate_t len_state;
	return mbrtowc(0, s, n, st ? st : &len_state);
}

int mbsinit(const mbstate_t *st)
{
	return !st || !st->__count;
}

size_t wcrtomb(char *restrict s, wchar_t wc, mbstate_t *restrict st)
{
	unsigned char *d = (unsigned char *)s;
	unsigned c = (unsigned)wc;
	if (!s) return 1;
	if (!__locale_utf8()) {
		if (c > 0xff) {
			errno = EILSEQ;
			return (size_t)-1;
		}
		*d = (unsigned char)c;
		return 1;
	}
	if (c < 0x80) {
		*d = (unsigned char)c;
		return 1;
	}
	if (c < 0x800) {
		d[0] = (unsigned char)(0xc0 | c >> 6);
		d[1] = (unsigned char)(0x80 | (c & 0x3f));
		return 2;
	}
	if (c < 0x10000) {
		if (c - 0xd800 < 0x800) goto ilseq;
		d[0] = (unsigned char)(0xe0 | c >> 12);
		d[1] = (unsigned char)(0x80 | (c >> 6 & 0x3f));
		d[2] = (unsigned char)(0x80 | (c & 0x3f));
		return 3;
	}
	if (c < 0x110000) {
		d[0] = (unsigned char)(0xf0 | c >> 18);
		d[1] = (unsigned char)(0x80 | (c >> 12 & 0x3f));
		d[2] = (unsigned char)(0x80 | (c >> 6 & 0x3f));
		d[3] = (unsigned char)(0x80 | (c & 0x3f));
		return 4;
	}
ilseq:
	errno = EILSEQ;
	return (size_t)-1;
}

wint_t btowc(int c)
{
	if (c == EOF) return WEOF;
	unsigned char b = (unsigned char)c;
	if (__locale_utf8() && b >= 0x80) return WEOF;
	return b;
}

int wctob(wint_t c)
{
	if (c < 0x80 || (!__locale_utf8() && c < 0x100)) return (int)c;
	return EOF;
}

int mbtowc(wchar_t *restrict wc, const char *restrict s, size_t n)
{
	static mbstate_t st;
	if (!s) {
		memset(&st, 0, sizeof st);
		return 0;                      /* encodings are stateless */
	}
	size_t r = mbrtowc(wc, s, n, &st);
	if (r == (size_t)-2) {
		memset(&st, 0, sizeof st);
		errno = EILSEQ;
		return -1;
	}
	return (int)r;
}

int mblen(const char *s, size_t n)
{
	return mbtowc(0, s, n);
}

int wctomb(char *s, wchar_t wc)
{
	if (!s) return 0;
	return (int)wcrtomb(s, wc, 0);
}

/* ---- strings ---- */

size_t mbsnrtowcs(wchar_t *restrict ws, const char **restrict src, size_t n, size_t wn,
	mbstate_t *restrict st)
{
	const char *s = *src;
	size_t cnt = 0;
	wchar_t tmp;
	if (!st) st = &internal_state;
	while (n && (!ws || cnt < wn)) {
		size_t r = mbrtowc(ws ? ws + cnt : &tmp, s, n, st);
		if (r == (size_t)-1) {
			if (ws) *src = s;
			return r;
		}
		if (r == (size_t)-2) {
			/* consumed the rest of the input into the state */
			s += n;
			n = 0;
			break;
		}
		if (r == 0) {
			if (ws) *src = 0;
			return cnt;
		}
		s += r;
		n -= r;
		cnt++;
	}
	if (ws) *src = s;
	return cnt;
}

size_t mbsrtowcs(wchar_t *restrict ws, const char **restrict src, size_t wn, mbstate_t *restrict st)
{
	return mbsnrtowcs(ws, (const char **)src, (size_t)-1, wn, st);
}

size_t wcsnrtombs(char *restrict d, const wchar_t **restrict src, size_t wn, size_t n,
	mbstate_t *restrict st)
{
	const wchar_t *ws = *src;
	size_t cnt = 0;
	char buf[MB_LEN_MAX];
	(void)st;
	for (; wn; wn--, ws++) {
		if (!*ws) {
			if (d) {
				if (cnt >= n) break;
				d[cnt] = 0;
				*src = 0;
			}
			return cnt;
		}
		size_t r = wcrtomb(buf, *ws, 0);
		if (r == (size_t)-1) {
			if (d) *src = ws;
			return r;
		}
		if (d) {
			if (cnt + r > n) break;
			memcpy(d + cnt, buf, r);
		}
		cnt += r;
	}
	if (d) *src = ws;
	return cnt;
}

size_t wcsrtombs(char *restrict d, const wchar_t **restrict src, size_t n, mbstate_t *restrict st)
{
	return wcsnrtombs(d, (const wchar_t **)src, (size_t)-1, n, st);
}

size_t mbstowcs(wchar_t *restrict ws, const char *restrict s, size_t n)
{
	mbstate_t st;
	const char *p = s;
	memset(&st, 0, sizeof st);
	return mbsrtowcs(ws, &p, n, &st);
}

size_t wcstombs(char *restrict s, const wchar_t *restrict ws, size_t n)
{
	const wchar_t *p = ws;
	return wcsrtombs(s, &p, n, 0);
}

/* ---- char16_t / char32_t (<uchar.h>) ----
 * char32_t is a code point; char16_t uses UTF-16, with surrogate pairs
 * carried across calls in the mbstate (__pending holds the pending low
 * surrogate for mbrtoc16, the pending high surrogate for c16rtomb). */

size_t mbrtoc32(char32_t *restrict c32, const char *restrict s, size_t n, mbstate_t *restrict st)
{
	static mbstate_t c32_state;
	wchar_t wc;
	if (!st) st = &c32_state;
	size_t r = mbrtowc(&wc, s, n, st);
	if (r <= 4 && c32 && s) *c32 = (char32_t)wc;
	return r;
}

size_t c32rtomb(char *restrict s, char32_t c32, mbstate_t *restrict st)
{
	return wcrtomb(s, (wchar_t)c32, st);
}

size_t mbrtoc16(char16_t *restrict c16, const char *restrict s, size_t n, mbstate_t *restrict st)
{
	static mbstate_t c16_state;
	if (!st) st = &c16_state;
	/* a low surrogate left from the previous call */
	if (st->__count == 0xff) {
		if (c16) *c16 = (char16_t)st->__pending;
		st->__count = st->__pending = 0;
		return (size_t)-3;
	}
	wchar_t wc;
	size_t r = mbrtowc(&wc, s, n, st);
	if (r > 4 || !s) return r;
	if ((unsigned)wc >= 0x10000) {
		unsigned v = (unsigned)wc - 0x10000;
		if (c16) *c16 = (char16_t)(0xd800 | v >> 10);
		st->__pending = 0xdc00 | (v & 0x3ff);
		st->__count = 0xff;
	} else if (c16) {
		*c16 = (char16_t)wc;
	}
	return r;
}

size_t c16rtomb(char *restrict s, char16_t c16, mbstate_t *restrict st)
{
	static mbstate_t c16_out;
	if (!st) st = &c16_out;
	if (!s) {
		int bad = st->__count == 0xfe;
		st->__count = st->__pending = 0;
		if (bad) {
			errno = EILSEQ;
			return (size_t)-1;
		}
		return 1;
	}
	unsigned c = c16;
	if (st->__count == 0xfe) {
		if (c - 0xdc00 >= 0x400) goto ilseq;
		unsigned cp = 0x10000 + ((st->__pending - 0xd800) << 10) + (c - 0xdc00);
		st->__count = st->__pending = 0;
		return wcrtomb(s, (wchar_t)cp, 0);
	}
	if (c - 0xd800 < 0x400) {
		st->__pending = c;
		st->__count = 0xfe;
		return 0;
	}
	if (c - 0xdc00 < 0x400) goto ilseq;
	return wcrtomb(s, (wchar_t)c, 0);
ilseq:
	st->__count = st->__pending = 0;
	errno = EILSEQ;
	return (size_t)-1;
}
