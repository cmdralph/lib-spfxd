/*
 * lib-spfxd — wide-character stream I/O.
 *
 * Streams always store bytes; a wide-oriented stream encodes and decodes
 * through the current locale's multibyte encoding (UTF-8 or the C locale's
 * byte encoding), keeping the conversion state in the FILE.
 *
 * The wprintf/wscanf families convert the wide format to multibyte and run
 * the byte-oriented engines, then convert the result (for swprintf and
 * swscanf) between wide and multibyte form.  Consequently field widths and
 * precisions applied to non-ASCII text count bytes of its multibyte
 * encoding, not wide characters; for ASCII text the two agree.
 */
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "stdio_impl.h"

int fwide(FILE *f, int mode)
{
	FLOCK(f);
	if (mode && !f->mode) f->mode = mode > 0 ? 1 : -1;
	int r = f->mode;
	FUNLOCK(f);
	return r;
}

static wint_t getwc_unlocked_impl(FILE *f)
{
	wchar_t wc;
	if (!f->mode) f->mode = 1;
	for (;;) {
		int c = getc_fast(f);
		if (c == EOF) {
			if (!mbsinit(&f->mbs)) {
				memset(&f->mbs, 0, sizeof f->mbs);
				f->flags |= F_ERR;
				errno = EILSEQ;
			}
			return WEOF;
		}
		char ch = (char)c;
		size_t r = mbrtowc(&wc, &ch, 1, &f->mbs);
		if (r == (size_t)-2) continue;
		if (r == (size_t)-1) {
			f->flags |= F_ERR;
			return WEOF;
		}
		return (wint_t)wc;
	}
}

wint_t fgetwc(FILE *f)
{
	FLOCK(f);
	wint_t c = getwc_unlocked_impl(f);
	FUNLOCK(f);
	return c;
}
weak_alias(fgetwc, getwc);

wint_t fgetwc_unlocked(FILE *f) { return getwc_unlocked_impl(f); }
weak_alias(fgetwc_unlocked, getwc_unlocked);
wint_t getwchar(void) { return fgetwc(&__stdin_FILE); }

static wint_t putwc_unlocked_impl(wchar_t wc, FILE *f)
{
	char mb[MB_LEN_MAX];
	if (!f->mode) f->mode = 1;
	size_t n = wcrtomb(mb, wc, 0);
	if (n == (size_t)-1) {
		f->flags |= F_ERR;
		return WEOF;
	}
	if (n == 1) return putc_fast(mb[0], f) == EOF ? WEOF : (wint_t)wc;
	return __fwritex((unsigned char *)mb, n, f) == n ? (wint_t)wc : WEOF;
}

wint_t fputwc(wchar_t wc, FILE *f)
{
	FLOCK(f);
	wint_t r = putwc_unlocked_impl(wc, f);
	FUNLOCK(f);
	return r;
}
weak_alias(fputwc, putwc);

wint_t fputwc_unlocked(wchar_t wc, FILE *f) { return putwc_unlocked_impl(wc, f); }
weak_alias(fputwc_unlocked, putwc_unlocked);
wint_t putwchar(wchar_t wc) { return fputwc(wc, &__stdout_FILE); }

wint_t ungetwc(wint_t c, FILE *f)
{
	char mb[MB_LEN_MAX];
	if (c == WEOF) return WEOF;
	size_t n = wcrtomb(mb, (wchar_t)c, 0);
	if (n == (size_t)-1) return WEOF;
	FLOCK(f);
	if (!f->mode) f->mode = 1;
	if (!f->rpos || f->wpos != f->wbase) __toread(f);
	if (!f->rpos || f->rpos - n < f->buf - UNGET) {
		FUNLOCK(f);
		return WEOF;
	}
	f->rpos -= n;
	memcpy(f->rpos, mb, n);
	f->flags &= ~F_EOF;
	FUNLOCK(f);
	return c;
}

wchar_t *fgetws(wchar_t *restrict s, int n, FILE *restrict f)
{
	wchar_t *p = s;
	if (n <= 0) return 0;
	FLOCK(f);
	while (--n > 0) {
		wint_t c = getwc_unlocked_impl(f);
		if (c == WEOF) break;
		*p++ = (wchar_t)c;
		if (c == '\n') break;
	}
	int err = (f->flags & F_ERR) != 0;
	FUNLOCK(f);
	if (p == s || err) return 0;
	*p = 0;
	return s;
}

int fputws(const wchar_t *restrict s, FILE *restrict f)
{
	FLOCK(f);
	for (; *s; s++) {
		if (putwc_unlocked_impl(*s, f) == WEOF) {
			FUNLOCK(f);
			return -1;
		}
	}
	FUNLOCK(f);
	return 0;
}

/* ---- formatted wide output ---- */

/* Convert a wide format to multibyte; returns malloc'd or small buffer. */
static char *narrow_fmt(const wchar_t *wf, char *small, size_t cap)
{
	mbstate_t st;
	memset(&st, 0, sizeof st);
	const wchar_t *src = wf;
	size_t need = wcsrtombs(0, &src, 0, &st);
	if (need == (size_t)-1) return 0;
	char *buf = need < cap ? small : malloc(need + 1);
	if (!buf) return 0;
	src = wf;
	memset(&st, 0, sizeof st);
	wcsrtombs(buf, &src, need + 1, &st);
	return buf;
}

int vswprintf(wchar_t *restrict ws, size_t n, const wchar_t *restrict wf, va_list ap)
{
	char small[256], *fmt = narrow_fmt(wf, small, sizeof small), *out;
	if (!fmt) return -1;
	int l = vasprintf(&out, fmt, ap);
	if (fmt != small) free(fmt);
	if (l < 0) return -1;
	const char *src = out;
	mbstate_t st;
	memset(&st, 0, sizeof st);
	size_t wl = mbsrtowcs(0, &src, 0, &st);
	int r;
	if (wl == (size_t)-1 || wl >= n) {
		/* does not fit (or not encodable): fail as ISO C requires */
		if (n) {
			src = out;
			memset(&st, 0, sizeof st);
			size_t k = mbsrtowcs(ws, &src, n - 1, &st);
			ws[k == (size_t)-1 ? 0 : k] = 0;
		}
		r = -1;
	} else {
		src = out;
		memset(&st, 0, sizeof st);
		mbsrtowcs(ws, &src, n, &st);
		r = (int)wl;
	}
	free(out);
	return r;
}

int swprintf(wchar_t *restrict ws, size_t n, const wchar_t *restrict wf, ...)
{
	va_list ap;
	va_start(ap, wf);
	int r = vswprintf(ws, n, wf, ap);
	va_end(ap);
	return r;
}

int vfwprintf(FILE *restrict f, const wchar_t *restrict wf, va_list ap)
{
	char small[256], *fmt = narrow_fmt(wf, small, sizeof small), *out;
	if (!fmt) return -1;
	int l = vasprintf(&out, fmt, ap);
	if (fmt != small) free(fmt);
	if (l < 0) return -1;
	size_t wl = mbstowcs(0, out, 0);
	FLOCK(f);
	if (!f->mode) f->mode = 1;
	size_t w = __fwritex((unsigned char *)out, (size_t)l, f);
	FUNLOCK(f);
	free(out);
	if (w != (size_t)l || wl == (size_t)-1 || wl > INT_MAX) return -1;
	return (int)wl;
}

int fwprintf(FILE *restrict f, const wchar_t *restrict wf, ...)
{
	va_list ap;
	va_start(ap, wf);
	int r = vfwprintf(f, wf, ap);
	va_end(ap);
	return r;
}

int vwprintf(const wchar_t *restrict wf, va_list ap)
{
	return vfwprintf(&__stdout_FILE, wf, ap);
}

int wprintf(const wchar_t *restrict wf, ...)
{
	va_list ap;
	va_start(ap, wf);
	int r = vfwprintf(&__stdout_FILE, wf, ap);
	va_end(ap);
	return r;
}

/* ---- formatted wide input ---- */

int vfwscanf(FILE *restrict f, const wchar_t *restrict wf, va_list ap)
{
	char small[256], *fmt = narrow_fmt(wf, small, sizeof small);
	if (!fmt) return EOF;
	FLOCK(f);
	if (!f->mode) f->mode = 1;
	int r = __vfscanf_core(f, fmt, ap);
	FUNLOCK(f);
	if (fmt != small) free(fmt);
	return r;
}

int fwscanf(FILE *restrict f, const wchar_t *restrict wf, ...)
{
	va_list ap;
	va_start(ap, wf);
	int r = vfwscanf(f, wf, ap);
	va_end(ap);
	return r;
}

int vwscanf(const wchar_t *restrict wf, va_list ap)
{
	return vfwscanf(&__stdin_FILE, wf, ap);
}

int wscanf(const wchar_t *restrict wf, ...)
{
	va_list ap;
	va_start(ap, wf);
	int r = vfwscanf(&__stdin_FILE, wf, ap);
	va_end(ap);
	return r;
}

int vswscanf(const wchar_t *restrict ws, const wchar_t *restrict wf, va_list ap)
{
	char small[256], *in = narrow_fmt(ws, small, sizeof small);
	char fsmall[256], *fmt = narrow_fmt(wf, fsmall, sizeof fsmall);
	int r = EOF;
	if (in && fmt) r = vsscanf(in, fmt, ap);
	if (in && in != small) free(in);
	if (fmt && fmt != fsmall) free(fmt);
	return r;
}

int swscanf(const wchar_t *restrict ws, const wchar_t *restrict wf, ...)
{
	va_list ap;
	va_start(ap, wf);
	int r = vswscanf(ws, wf, ap);
	va_end(ap);
	return r;
}
