/*
 * lib-spfxd — the scanf engine.
 *
 * Supports every ISO C conversion (d i o u x X a e f g A E F G c s [ p n %),
 * assignment suppression '*', field widths, length modifiers hh h l ll q j
 * z t L, POSIX positional arguments (%n$) and the POSIX 'm' allocation
 * modifier (%ms, %mc, %m[...]), plus %lc, %ls and %l[ which store wide
 * characters decoded from the multibyte input.
 *
 * Input is read straight out of the stream buffer.  Numbers are first
 * collected as the longest prefix that can begin a valid numeral, then
 * converted with the strtol/strtod machinery.  As C11 7.21.6.2p9 requires,
 * an input item that is only the prefix of a numeral ("1.5e", "0x", "-")
 * is consumed and the directive fails.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "stdio_impl.h"

enum { L_NONE, L_HH, L_H, L_L, L_LL, L_J, L_Z, L_T, L_BIGL };


struct in {
	FILE *f;
	long long count;   /* characters consumed (for %n) */
	long width;        /* remaining field width, or LONG_MAX */
};

static int in_get(struct in *in)
{
	if (in->width <= 0) return EOF;
	int c = getc_fast(in->f);
	if (c != EOF) {
		in->count++;
		in->width--;
	}
	return c;
}

/* Undo the last in_get (only ever called right after it returned c). */
static void in_unget(struct in *in, int c)
{
	if (c == EOF) return;
	in->f->rpos--;
	in->count--;
	in->width++;
}

static void skip_space(struct in *in)
{
	int c;
	long w = in->width;
	in->width = LONG_MAX;
	while (isspace(c = in_get(in)));
	in_unget(in, c);
	in->width = w;
}

/* Growable collection buffer for numerals. */
struct buf {
	char *p;
	size_t n, cap;
	char small[128];
	int oom;
};

static void buf_put(struct buf *b, char c)
{
	if (b->n + 1 >= b->cap) {
		size_t cap = b->cap * 2;
		char *np = b->p == b->small ? malloc(cap) : realloc(b->p, cap);
		if (!np) {
			b->oom = 1;
			return;
		}
		if (b->p == b->small) memcpy(np, b->small, b->n);
		b->p = np;
		b->cap = cap;
	}
	b->p[b->n++] = c;
	b->p[b->n] = 0;
}

static void buf_free(struct buf *b)
{
	if (b->p != b->small) free(b->p);
}

static int hexdig(int c) { return (unsigned)(c - '0') < 10 || (unsigned)((c | 32) - 'a') < 6; }

/* Collect an integer numeral for the given base (0 = auto). */
static void collect_int(struct in *in, struct buf *b, int base)
{
	int c = in_get(in);
	if (c == '+' || c == '-') {
		buf_put(b, (char)c);
		c = in_get(in);
	}
	if ((base == 0 || base == 16) && c == '0') {
		buf_put(b, '0');
		c = in_get(in);
		if ((c | 32) == 'x') {
			buf_put(b, (char)c);
			c = in_get(in);
			base = 16;
		} else if (base == 0) {
			base = 8;
		}
	}
	if (!base) base = 10;
	for (;; c = in_get(in)) {
		int ok = base == 16 ? hexdig(c) : base == 8 ? (unsigned)(c - '0') < 8 : (unsigned)(c - '0') < 10;
		if (!ok) break;
		buf_put(b, (char)c);
	}
	in_unget(in, c);
}

static int ci_eq(int c, char lower)
{
	return (c | 32) == lower;
}

/* Collect a floating-point numeral. */
static void collect_float(struct in *in, struct buf *b)
{
	int c = in_get(in);
	if (c == '+' || c == '-') {
		buf_put(b, (char)c);
		c = in_get(in);
	}
	if (ci_eq(c, 'i') || ci_eq(c, 'n')) {
		const char *word = ci_eq(c, 'i') ? "infinity" : "nan";
		size_t i = 0;
		for (; word[i] && ci_eq(c, word[i]); i++, c = in_get(in)) buf_put(b, (char)c);
		if (word[0] == 'n' && i == 3 && c == '(') {
			buf_put(b, '(');
			while ((c = in_get(in)) != EOF && (isalnum(c) || c == '_')) buf_put(b, (char)c);
			if (c == ')') {
				buf_put(b, ')');
				return;
			}
		}
		in_unget(in, c);
		return;
	}
	int hex = 0, digits = 0, point = 0;
	if (c == '0') {
		buf_put(b, '0');
		digits = 1;
		c = in_get(in);
		if ((c | 32) == 'x') {
			buf_put(b, (char)c);
			c = in_get(in);
			hex = 1;
		}
	}
	for (;; c = in_get(in)) {
		if (c == '.' && !point) {
			point = 1;
		} else if (hex ? hexdig(c) : (unsigned)(c - '0') < 10) {
			digits = 1;
		} else {
			break;
		}
		buf_put(b, (char)c);
	}
	if (digits && (c | 32) == (hex ? 'p' : 'e')) {
		buf_put(b, (char)c);
		c = in_get(in);
		if (c == '+' || c == '-') {
			buf_put(b, (char)c);
			c = in_get(in);
		}
		for (; (unsigned)(c - '0') < 10; c = in_get(in)) buf_put(b, (char)c);
	}
	in_unget(in, c);
}

static void store_int(void *p, int len, uintmax_t v)
{
	switch (len) {
	case L_HH: *(char *)p = (char)v; break;
	case L_H: *(short *)p = (short)v; break;
	case L_L: *(long *)p = (long)v; break;
	case L_LL: *(long long *)p = (long long)v; break;
	case L_J: *(uintmax_t *)p = v; break;
	case L_Z: *(size_t *)p = (size_t)v; break;
	case L_T: *(ptrdiff_t *)p = (ptrdiff_t)v; break;
	default: *(int *)p = (int)v; break;
	}
}

static void *pos_arg(va_list ap, int n)
{
	va_list a2;
	void *p = 0;
	va_copy(a2, ap);
	for (int i = 0; i < n; i++) p = va_arg(a2, void *);
	va_end(a2);
	return p;
}

/* Scanset for %[...]: one bit per byte value. */
static const char *parse_set(const char *p, unsigned char set[32])
{
	int invert = 0;
	memset(set, 0, 32);
	if (*p == '^') {
		invert = 1;
		p++;
	}
	if (*p == ']') {
		set[']' >> 3] |= 1 << (']' & 7);
		p++;
	}
	for (; *p && *p != ']'; p++) {
		unsigned char a = (unsigned char)*p;
		if (p[1] == '-' && p[2] && p[2] != ']') {
			unsigned char z = (unsigned char)p[2];
			for (unsigned c = a; c <= z; c++) set[c >> 3] |= (unsigned char)(1 << (c & 7));
			p += 2;
		} else {
			set[a >> 3] |= (unsigned char)(1 << (a & 7));
		}
	}
	if (invert)
		for (int i = 0; i < 32; i++) set[i] = (unsigned char)~set[i];
	return *p ? p : 0;
}

#define IN_SET(set, c) ((set)[(unsigned char)(c) >> 3] & (1 << ((unsigned char)(c) & 7)))

hidden int __vfscanf_core(FILE *restrict f, const char *restrict fmt, va_list ap)
{
	struct in in = { f, 0, LONG_MAX };
	int matches = 0, input_failure = 0;
	const unsigned char *p;
	unsigned char set[32];

	if (!f->mode) f->mode = -1;

	for (p = (const unsigned char *)fmt; *p; p++) {
		in.width = LONG_MAX;
		if (isspace(*p)) {
			while (isspace(p[1])) p++;
			skip_space(&in);
			continue;
		}
		if (*p != '%' || p[1] == '%') {
			if (*p == '%') {
				p++;
				skip_space(&in);
			}
			int c = in_get(&in);
			if (c != *p) {
				in_unget(&in, c);
				if (c == EOF) input_failure = 1;
				goto done;
			}
			continue;
		}

		p++;
		void *dest = 0;
		int suppress = 0, alloc = 0, len = L_NONE;
		long width = 0;

		if (*p == '*') {
			suppress = 1;
			p++;
		} else if (isdigit(*p) && p[1] == '$') {
			dest = pos_arg(ap, *p - '0');
			p += 2;
		} else if (isdigit(*p) && isdigit(p[1]) && p[2] == '$') {
			dest = pos_arg(ap, (*p - '0') * 10 + (p[1] - '0'));
			p += 3;
		}
		for (; isdigit(*p); p++) width = width * 10 + (*p - '0');
		if (*p == 'm') {
			alloc = 1;
			p++;
		}
		switch (*p) {
		case 'h': p++; len = L_H; if (*p == 'h') { p++; len = L_HH; } break;
		case 'l': p++; len = L_L; if (*p == 'l') { p++; len = L_LL; } break;
		case 'q': p++; len = L_LL; break;
		case 'j': p++; len = L_J; break;
		case 'z': p++; len = L_Z; break;
		case 't': p++; len = L_T; break;
		case 'L': p++; len = L_BIGL; break;
		}
		int conv = *p;
		if (!conv) goto done;
		if (conv == 'C') { conv = 'c'; len = L_L; }
		if (conv == 'S') { conv = 's'; len = L_L; }
		if (!suppress && !dest && conv != 'n') dest = va_arg(ap, void *);
		if (conv == 'n') {
			if (!suppress) {
				if (!dest) dest = va_arg(ap, void *);
				store_int(dest, len, (uintmax_t)in.count);
			}
			continue;
		}
		if (conv != '[' && conv != 'c') skip_space(&in);

		/* probe for end of input before the conversion starts */
		in.width = LONG_MAX;
		int c0 = in_get(&in);
		if (c0 == EOF) {
			input_failure = 1;
			goto done;
		}
		in_unget(&in, c0);
		in.width = width ? width : (conv == 'c' ? 1 : LONG_MAX);

		switch (conv) {
		case 'c': case 's': case '[': {
			if (conv == '[') {
				const char *e = parse_set((const char *)p + 1, set);
				if (!e) goto done;
				p = (const unsigned char *)e;
			}
			int wide = len == L_L;
			size_t n = 0, cap = 0;
			char *out = 0;
			wchar_t *wout = 0;
			char **adest = 0;
			mbstate_t st;
			memset(&st, 0, sizeof st);
			if (!suppress) {
				if (alloc) {
					adest = dest;
					cap = conv == 'c' ? (size_t)in.width : 32;
					void *m = malloc(cap * (wide ? sizeof(wchar_t) : 1) + (wide ? sizeof(wchar_t) : 1));
					if (!m) goto done;
					if (wide) wout = m;
					else out = m;
				} else if (wide) {
					wout = dest;
				} else {
					out = dest;
				}
			}
			int c;
			for (;;) {
				if (in.width <= 0) break;
				c = in_get(&in);
				if (c == EOF) break;
				if ((conv == 's' && isspace(c)) || (conv == '[' && !IN_SET(set, c))) {
					in_unget(&in, c);
					break;
				}
				if (alloc && !suppress && n + 1 >= cap) {
					size_t ncap = cap * 2 + 1;
					void *m = realloc(wide ? (void *)wout : (void *)out,
						ncap * (wide ? sizeof(wchar_t) : 1) + (wide ? sizeof(wchar_t) : 1));
					if (!m) {
						free(wide ? (void *)wout : (void *)out);
						goto done;
					}
					if (wide) wout = m;
					else out = m;
					cap = ncap;
				}
				if (wide) {
					/* gather one multibyte character */
					char ch = (char)c;
					wchar_t wc;
					size_t r = mbrtowc(&wc, &ch, 1, &st);
					if (r == (size_t)-1) {
						errno = EILSEQ;
						goto done;
					}
					if (r == (size_t)-2) continue;
					if (wout) wout[n] = wc;
					n++;
				} else {
					if (out) out[n] = (char)c;
					n++;
				}
			}
			if (!n) {
				if (alloc && !suppress) free(wide ? (void *)wout : (void *)out);
				goto done;
			}
			if (conv == 'c' && n < (size_t)(width ? width : 1)) {
				/* %c needs exactly `width` characters */
				if (alloc && !suppress) free(wide ? (void *)wout : (void *)out);
				input_failure = 1;
				goto done;
			}
			if (!suppress) {
				if (conv != 'c') {
					if (wide) wout[n] = 0;
					else out[n] = 0;
				}
				if (alloc) *adest = wide ? (char *)(void *)wout : out;
				matches++;
			}
			break;
		}
		case 'd': case 'i': case 'o': case 'u': case 'x': case 'X': case 'p': {
			struct buf b = { 0 };
			b.p = b.small;
			b.cap = sizeof b.small;
			int base = conv == 'd' || conv == 'u' ? 10 : conv == 'i' ? 0 : conv == 'o' ? 8 : 16;
			collect_int(&in, &b, base);
			if (b.oom) {
				buf_free(&b);
				goto done;
			}
			char *end;
			int neg, ovf;
			uintmax_t v = 0;
			if (b.n) {
				uintmax_t lim = UINTMAX_MAX, nlim = UINTMAX_MAX;
				if (conv == 'd' || conv == 'i') {
					lim = INTMAX_MAX;
					nlim = (uintmax_t)INTMAX_MAX + 1;
				}
				v = __strto_core(b.p, &end, base, lim, nlim, &neg, &ovf);
				if (neg) v = 0 - v;
			} else {
				end = b.p;
			}
			size_t used = (size_t)(end - b.p);
			buf_free(&b);
			/* the input item is consumed; if it is only a prefix of a
			 * numeral (a lone sign, "0x") the directive fails (C11
			 * 7.21.6.2p9) */
			if (!used || used < b.n) goto done;
			if (!suppress) {
				if (conv == 'p') *(void **)dest = (void *)(uintptr_t)v;
				else store_int(dest, len, v);
				matches++;
			}
			break;
		}
		case 'a': case 'e': case 'f': case 'g':
		case 'A': case 'E': case 'F': case 'G': {
			struct buf b = { 0 };
			b.p = b.small;
			b.cap = sizeof b.small;
			collect_float(&in, &b);
			if (b.oom) {
				buf_free(&b);
				goto done;
			}
			char *end = b.p;
			long double v = b.n ? __strtold_internal(b.p, &end,
				len == L_BIGL ? 2 : len == L_L ? 1 : 0) : 0;
			size_t used = (size_t)(end - b.p);
			buf_free(&b);
			/* "1.5e", "0x", "-": a prefix of a numeral that is not one
			 * is a matching failure (C11 7.21.6.2p9, the "100ergs"
			 * example); the input item stays consumed */
			if (!used || used < b.n) goto done;
			if (!suppress) {
				if (len == L_BIGL) *(long double *)dest = v;
				else if (len == L_L) *(double *)dest = (double)v;
				else *(float *)dest = (float)v;
				matches++;
			}
			break;
		}
		default:
			goto done;
		}
	}
done:
	if (input_failure && !matches) {
		/* distinguish "nothing matched" from "input ended first" */
		return EOF;
	}
	return matches;
}

int vfscanf(FILE *restrict f, const char *restrict fmt, va_list ap)
{
	FLOCK(f);
	int r = __vfscanf_core(f, fmt, ap);
	FUNLOCK(f);
	return r;
}

int fscanf(FILE *restrict f, const char *restrict fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = vfscanf(f, fmt, ap);
	va_end(ap);
	return r;
}

int vscanf(const char *restrict fmt, va_list ap)
{
	return vfscanf(&__stdin_FILE, fmt, ap);
}

int scanf(const char *restrict fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = vfscanf(&__stdin_FILE, fmt, ap);
	va_end(ap);
	return r;
}

static size_t string_read(FILE *f, unsigned char *buf, size_t n)
{
	f->flags |= F_EOF;
	return 0;
}

int vsscanf(const char *restrict s, const char *restrict fmt, va_list ap)
{
	FILE f;
	size_t len = strlen(s);
	/* The buffer is the string's end so an attempted refill at end of
	 * input leaves the read pointers there. */
	__string_file_init(&f, (unsigned char *)s + len, 0);
	f.flags |= F_NOWR | F_STR;
	f.read = string_read;
	f.rpos = (unsigned char *)s;
	f.rend = (unsigned char *)s + len;
	return __vfscanf_core(&f, fmt, ap);
}

int sscanf(const char *restrict s, const char *restrict fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int r = vsscanf(s, fmt, ap);
	va_end(ap);
	return r;
}
