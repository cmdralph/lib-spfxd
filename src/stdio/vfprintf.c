/*
 * lib-spfxd — the printf formatting engine.
 *
 * Supports every ISO C conversion (d i o u x X c s p n f F e E g G a A %),
 * the flags - + space # 0 ', field width and precision (literal, `*`, or
 * positional `*n$`), length modifiers hh h l ll q j z Z t L, positional
 * arguments (%n$), C23 %b/%B, %lc/%ls/%C/%S and GNU %m.
 *
 * Structure: each conversion specification is parsed into a `struct spec`,
 * its argument fetched, and a type-specific emitter renders it.  All
 * emitters share one layout routine (emit_field) that places
 *   [spaces][sign/prefix][zeros][body][spaces]
 * according to width, precision and flags.
 *
 * Arguments are consumed directly with va_arg in a single pass.  Only a
 * format that actually uses positional arguments is scanned twice: a dry
 * run records the type of every position, the arguments are fetched in
 * order, and the output pass reads them from that array.
 *
 * Integers are converted right-to-left two digits per step from a digit
 * pair table; the divisions by 100 compile to multiplications.  Output is
 * copied straight into the stream buffer (for sprintf that buffer is the
 * caller's destination array).
 */
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include "stdio_impl.h"


enum { L_NONE, L_HH, L_H, L_L, L_LL, L_J, L_Z, L_T, L_BIGL };

enum {
	T_NONE, T_INT, T_UINT, T_LONG, T_ULONG, T_LLONG, T_ULLONG,
	T_SHORT, T_USHORT, T_CHAR, T_UCHAR, T_SIZE, T_IMAX, T_UMAX, T_PDIFF,
	T_PTR, T_DBL, T_LDBL
};

#define NL_MAX 64

union arg {
	uintmax_t i;
	long double f;
	void *p;
};

struct spec {
	unsigned fl;
	int width;
	int prec;          /* -1 when absent */
	int len;
	int argpos;        /* 0 for sequential */
	int wpos, ppos;    /* positional width / precision argument, or 0 */
	int wstar, pstar;  /* width / precision come from an argument */
	char conv;
};

static const char digit_pairs[200] =
	"00010203040506070809101112131415161718192021222324"
	"25262728293031323334353637383940414243444546474849"
	"50515253545556575859606162636465666768697071727374"
	"75767778798081828384858687888990919293949596979899";

/* ---------------------------------------------------------------------- */
/* output primitives                                                       */
/* ---------------------------------------------------------------------- */

static void out(FILE *f, const char *s, size_t l)
{
	if (!l || (f->flags & F_ERR)) return;
	if (f->lbf < 0 && (size_t)(f->wend - f->wpos) >= l) {
		memcpy(f->wpos, s, l);
		f->wpos += l;
		return;
	}
	__fwritex((const unsigned char *)s, l, f);
}

static void repeat(FILE *f, char c, long n)
{
	char buf[64];
	if (n <= 0) return;
	memset(buf, c, n < 64 ? (size_t)n : 64);
	for (; n > 64; n -= 64) out(f, buf, 64);
	out(f, buf, (size_t)n);
}

/*
 * Lay out one field: `pre` (sign or radix prefix), `zeros` precision zeros,
 * then `body`.  Width padding uses spaces on the left (default), spaces on
 * the right (FL_LEFT) or zeros between prefix and body (FL_ZERO).
 * Returns the field width written, or -1 if it exceeds INT_MAX.
 */
static long emit_field(FILE *f, const struct spec *sp, const char *pre, size_t pl,
	long zeros, const char *body, size_t bl)
{
	long content = (long)pl + zeros + (long)bl;
	long width = sp->width > content ? sp->width : content;
	long fill = width - content;
	if (width > INT_MAX) return -1;
	if (!(sp->fl & FL_LEFT) && !(sp->fl & FL_ZERO)) repeat(f, ' ', fill);
	out(f, pre, pl);
	if (!(sp->fl & FL_LEFT) && (sp->fl & FL_ZERO)) repeat(f, '0', fill);
	repeat(f, '0', zeros);
	out(f, body, bl);
	if (sp->fl & FL_LEFT) repeat(f, ' ', fill);
	return width;
}

/* ---------------------------------------------------------------------- */
/* integer conversion                                                      */
/* ---------------------------------------------------------------------- */

static char *to_dec(uintmax_t x, char *end)
{
	while (x >= 100) {
		unsigned r = (unsigned)(x % 100);
		x /= 100;
		end -= 2;
		memcpy(end, digit_pairs + 2 * r, 2);
	}
	if (x >= 10) {
		end -= 2;
		memcpy(end, digit_pairs + 2 * x, 2);
	} else {
		*--end = (char)('0' + x);
	}
	return end;
}

static char *to_pow2(uintmax_t x, char *end, unsigned shift, int upper)
{
	static const char lo[] = "0123456789abcdef", up[] = "0123456789ABCDEF";
	const char *d = upper ? up : lo;
	unsigned mask = (1U << shift) - 1;
	do {
		*--end = d[x & mask];
		x >>= shift;
	} while (x);
	return end;
}

static long fmt_integer(FILE *f, struct spec *sp, uintmax_t v)
{
	char buf[sizeof(uintmax_t) * 8 + 2], *end = buf + sizeof buf, *digits;
	char pre[3];
	size_t pl = 0;
	int neg = 0;

	switch (sp->conv) {
	case 'd': case 'i':
		if ((intmax_t)v < 0) {
			neg = 1;
			v = 0 - v;
		}
		digits = to_dec(v, end);
		if (neg) pre[pl++] = '-';
		else if (sp->fl & FL_PLUS) pre[pl++] = '+';
		else if (sp->fl & FL_SPACE) pre[pl++] = ' ';
		break;
	case 'u':
		digits = to_dec(v, end);
		break;
	case 'o':
		digits = to_pow2(v, end, 3, 0);
		break;
	case 'x': case 'X': case 'p':
		digits = to_pow2(v, end, 4, sp->conv == 'X');
		if ((v && (sp->fl & FL_ALT)) || sp->conv == 'p') {
			pre[pl++] = '0';
			pre[pl++] = sp->conv == 'X' ? 'X' : 'x';
		}
		break;
	default: /* b, B */
		digits = to_pow2(v, end, 1, 0);
		if (v && (sp->fl & FL_ALT)) {
			pre[pl++] = '0';
			pre[pl++] = sp->conv;
		}
		break;
	}

	size_t nd = (size_t)(end - digits);
	long zeros = 0;
	if (sp->prec >= 0) {
		sp->fl &= ~FL_ZERO;
		if (v == 0 && sp->prec == 0) nd = 0, digits = end;
		if ((long)nd < sp->prec) zeros = sp->prec - (long)nd;
	}
	/* "#o" guarantees a leading zero */
	if (sp->conv == 'o' && (sp->fl & FL_ALT) && zeros == 0 && (nd == 0 || digits[0] != '0'))
		zeros = 1;
	return emit_field(f, sp, pre, pl, zeros, digits, nd);
}

/* ---------------------------------------------------------------------- */
/* strings and characters                                                  */
/* ---------------------------------------------------------------------- */

static long fmt_string(FILE *f, struct spec *sp, const char *s)
{
	if (!s) s = (sp->prec >= 0 && sp->prec < 6) ? "" : "(null)";
	size_t n = sp->prec >= 0 ? strnlen(s, (size_t)sp->prec) : strlen(s);
	if (n > INT_MAX) return -1;
	sp->fl &= ~FL_ZERO;
	return emit_field(f, sp, "", 0, 0, s, n);
}

/* %ls: the precision bounds the number of bytes written and never splits a
 * multibyte character.  Returns -2 on an unencodable wide character. */
static long fmt_wstring(FILE *f, struct spec *sp, const wchar_t *ws)
{
	char mb[MB_LEN_MAX];
	mbstate_t st;
	size_t total = 0, lim = sp->prec >= 0 ? (size_t)sp->prec : (size_t)INT_MAX + 1;
	const wchar_t *s;

	if (!ws) return fmt_string(f, sp, 0);
	memset(&st, 0, sizeof st);
	for (s = ws; *s; s++) {
		size_t k = wcrtomb(mb, *s, &st);
		if (k == (size_t)-1) return -2;
		if (total + k > lim) break;
		total += k;
	}
	if (total > INT_MAX) return -1;
	long width = sp->width > (long)total ? sp->width : (long)total;
	if (!(sp->fl & FL_LEFT)) repeat(f, ' ', width - (long)total);
	memset(&st, 0, sizeof st);
	for (s = ws, lim = 0; lim < total; s++) {
		size_t k = wcrtomb(mb, *s, &st);
		out(f, mb, k);
		lim += k;
	}
	if (sp->fl & FL_LEFT) repeat(f, ' ', width - (long)total);
	return width;
}

/* ---------------------------------------------------------------------- */
/* specification parsing                                                   */
/* ---------------------------------------------------------------------- */

/* Decimal number; -1 if it exceeds INT_MAX. */
static int parse_num(const char **ps)
{
	const char *s = *ps;
	int v = 0;
	for (; (unsigned)(*s - '0') < 10; s++) {
		if (v >= 0 && v <= (INT_MAX - (*s - '0')) / 10) v = v * 10 + (*s - '0');
		else v = -1;
	}
	*ps = s;
	return v;
}

/* "n$" after a run of digits; returns n (1..NL_MAX) or 0 if absent, -1 bad. */
static int parse_pos(const char **ps)
{
	const char *s = *ps;
	if ((unsigned)(*s - '0') >= 10) return 0;
	int n = parse_num(&s);
	if (*s != '$') return 0;
	if (n < 1 || n > NL_MAX) return -1;
	*ps = s + 1;
	return n;
}

/* Parse the specification following '%'.  Returns 0 or -1 (EINVAL/EOVERFLOW
 * already stored in *err). */
static int parse_spec(const char **ps, struct spec *sp, int *err)
{
	const char *s = *ps;
	memset(sp, 0, sizeof *sp);
	sp->prec = -1;

	sp->argpos = parse_pos(&s);
	if (sp->argpos < 0) goto inval;

	for (;; s++) {
		switch (*s) {
		case '-': sp->fl |= FL_LEFT; continue;
		case '0': sp->fl |= FL_ZERO; continue;
		case '+': sp->fl |= FL_PLUS; continue;
		case ' ': sp->fl |= FL_SPACE; continue;
		case '#': sp->fl |= FL_ALT; continue;
		case '\'': sp->fl |= FL_GROUP; continue;
		}
		break;
	}

	if (*s == '*') {
		s++;
		sp->wstar = 1;
		sp->wpos = parse_pos(&s);
		if (sp->wpos < 0) goto inval;
	} else if ((unsigned)(*s - '0') < 10) {
		sp->width = parse_num(&s);
		if (sp->width < 0) goto overflow;
	}

	if (*s == '.') {
		s++;
		if (*s == '*') {
			s++;
			sp->pstar = 1;
			sp->ppos = parse_pos(&s);
			if (sp->ppos < 0) goto inval;
		} else {
			sp->prec = parse_num(&s);
			if (sp->prec < 0) goto overflow;
		}
	}

	switch (*s) {
	case 'h': s++; sp->len = L_H; if (*s == 'h') { s++; sp->len = L_HH; } break;
	case 'l': s++; sp->len = L_L; if (*s == 'l') { s++; sp->len = L_LL; } break;
	case 'q': s++; sp->len = L_LL; break;
	case 'L': s++; sp->len = L_BIGL; break;
	case 'j': s++; sp->len = L_J; break;
	case 'z': case 'Z': s++; sp->len = L_Z; break;
	case 't': s++; sp->len = L_T; break;
	}
	sp->conv = *s;
	if (!*s) goto inval;
	*ps = s + 1;
	return 0;
inval:
	*err = EINVAL;
	return -1;
overflow:
	*err = EOVERFLOW;
	return -1;
}

static int arg_type(const struct spec *sp)
{
	int len = sp->len;
	switch (sp->conv) {
	case 'd': case 'i':
		switch (len) {
		case L_HH: return T_CHAR;
		case L_H: return T_SHORT;
		case L_L: return T_LONG;
		case L_LL: return T_LLONG;
		case L_J: return T_IMAX;
		case L_Z: return T_SIZE;
		case L_T: return T_PDIFF;
		}
		return T_INT;
	case 'o': case 'u': case 'x': case 'X': case 'b': case 'B':
		switch (len) {
		case L_HH: return T_UCHAR;
		case L_H: return T_USHORT;
		case L_L: return T_ULONG;
		case L_LL: return T_ULLONG;
		case L_J: return T_UMAX;
		case L_Z: return T_SIZE;
		case L_T: return T_PDIFF;
		}
		return T_UINT;
	case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
		return len == L_BIGL ? T_LDBL : T_DBL;
	case 'c': return len == L_L ? T_UINT : T_INT;
	case 'C': return T_UINT;
	case 's': case 'S': case 'p': case 'n': return T_PTR;
	case '%': case 'm': return T_NONE;
	}
	return -1;
}

static void fetch(union arg *a, int type, va_list *ap)
{
	switch (type) {
	case T_PTR:    a->p = va_arg(*ap, void *); break;
	case T_INT:    a->i = (uintmax_t)(intmax_t)va_arg(*ap, int); break;
	case T_UINT:   a->i = va_arg(*ap, unsigned int); break;
	case T_LONG:   a->i = (uintmax_t)(intmax_t)va_arg(*ap, long); break;
	case T_ULONG:  a->i = va_arg(*ap, unsigned long); break;
	case T_LLONG:  a->i = (uintmax_t)(intmax_t)va_arg(*ap, long long); break;
	case T_ULLONG: a->i = va_arg(*ap, unsigned long long); break;
	case T_SHORT:  a->i = (uintmax_t)(intmax_t)(short)va_arg(*ap, int); break;
	case T_USHORT: a->i = (unsigned short)va_arg(*ap, int); break;
	case T_CHAR:   a->i = (uintmax_t)(intmax_t)(signed char)va_arg(*ap, int); break;
	case T_UCHAR:  a->i = (unsigned char)va_arg(*ap, int); break;
	case T_SIZE:   a->i = va_arg(*ap, size_t); break;
	case T_IMAX:   a->i = (uintmax_t)va_arg(*ap, intmax_t); break;
	case T_UMAX:   a->i = va_arg(*ap, uintmax_t); break;
	case T_PDIFF:  a->i = (uintmax_t)(intmax_t)va_arg(*ap, ptrdiff_t); break;
	case T_DBL:    a->f = va_arg(*ap, double); break;
	case T_LDBL:   a->f = va_arg(*ap, long double); break;
	}
}

static void store_count(void *p, int len, long cnt)
{
	switch (len) {
	case L_HH: *(signed char *)p = (signed char)cnt; break;
	case L_H:  *(short *)p = (short)cnt; break;
	case L_L:  *(long *)p = cnt; break;
	case L_LL: *(long long *)p = cnt; break;
	case L_J:  *(intmax_t *)p = cnt; break;
	case L_Z:  *(size_t *)p = (size_t)cnt; break;
	case L_T:  *(ptrdiff_t *)p = cnt; break;
	default:   *(int *)p = (int)cnt; break;
	}
}

/* ---------------------------------------------------------------------- */
/* positional dry run                                                      */
/* ---------------------------------------------------------------------- */

/* Record argument types of a positional format.  Returns 1 if the format
 * is positional, 0 if it is sequential, -1 if it is invalid. */
static int collect_positional(const char *s, int *types, int *err)
{
	struct spec sp;
	int positional = -1;
	while ((s = strchr(s, '%'))) {
		s++;
		if (*s == '%') { s++; continue; }
		if (parse_spec(&s, &sp, err)) return -1;
		int t = arg_type(&sp);
		if (t < 0) { *err = EINVAL; return -1; }
		int uses_args = t != T_NONE || sp.wstar || sp.pstar;
		if (!uses_args) continue;
		int pos = sp.argpos > 0 || (t == T_NONE && (sp.wpos || sp.ppos));
		if (positional < 0) {
			positional = pos;
			if (!pos) return 0;
		}
		if (!pos || (t != T_NONE && !sp.argpos) || (sp.wstar && !sp.wpos) || (sp.pstar && !sp.ppos)) {
			*err = EINVAL;
			return -1;
		}
		if (t != T_NONE) types[sp.argpos] = t;
		if (sp.wpos) types[sp.wpos] = T_INT;
		if (sp.ppos) types[sp.ppos] = T_INT;
	}
	return positional > 0;
}

/* ---------------------------------------------------------------------- */
/* driver                                                                  */
/* ---------------------------------------------------------------------- */

static int format(FILE *f, const char *s, va_list *ap, const union arg *pos_args)
{
	long cnt = 0, w;
	int err = 0, saved_errno = errno;
	struct spec sp;
	union arg arg;
	wchar_t wc[2];

	while (*s) {
		if (*s != '%') {
			const char *e = strchrnul(s, '%');
			if (e - s > INT_MAX - cnt) goto overflow;
			out(f, s, (size_t)(e - s));
			cnt += e - s;
			s = e;
			continue;
		}
		if (s[1] == '%') {
			out(f, "%", 1);
			cnt++;
			s += 2;
			continue;
		}
		s++;
		if (parse_spec(&s, &sp, &err)) goto fail;
		int t = arg_type(&sp);
		if (t < 0) { err = EINVAL; goto fail; }

		if (sp.wstar) {
			sp.width = sp.wpos ? (int)pos_args[sp.wpos].i : va_arg(*ap, int);
			if (sp.width < 0) {
				if (sp.width == INT_MIN) goto overflow;
				sp.fl |= FL_LEFT;
				sp.width = -sp.width;
			}
		}
		if (sp.pstar) {
			sp.prec = sp.ppos ? (int)pos_args[sp.ppos].i : va_arg(*ap, int);
			if (sp.prec < 0) sp.prec = -1;
		}
		if (t != T_NONE) {
			if (sp.argpos) arg = pos_args[sp.argpos];
			else fetch(&arg, t, ap);
		}
		if (sp.fl & FL_LEFT) sp.fl &= ~FL_ZERO;
		if (sp.fl & FL_PLUS) sp.fl &= ~FL_SPACE;

		switch (sp.conv) {
		case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': case 'b': case 'B':
			w = fmt_integer(f, &sp, arg.i);
			break;
		case 'p':
			if (!arg.p) {
				sp.prec = -1;
				w = fmt_string(f, &sp, "(nil)");
			} else {
				sp.fl |= FL_ALT;
				w = fmt_integer(f, &sp, (uintptr_t)arg.p);
			}
			break;
		case 'c':
			if (sp.len == L_L) goto wide_char;
			{
				char c = (char)arg.i;
				sp.fl &= ~FL_ZERO;
				w = emit_field(f, &sp, "", 0, 0, &c, 1);
			}
			break;
		case 'C':
		wide_char:
			wc[0] = (wchar_t)arg.i;
			wc[1] = 0;
			sp.prec = -1;
			w = fmt_wstring(f, &sp, wc);
			break;
		case 's':
			w = sp.len == L_L ? fmt_wstring(f, &sp, arg.p) : fmt_string(f, &sp, arg.p);
			break;
		case 'S':
			w = fmt_wstring(f, &sp, arg.p);
			break;
		case 'm':
			w = fmt_string(f, &sp, strerror(saved_errno));
			break;
		case 'n':
			store_count(arg.p, sp.len, cnt);
			w = 0;
			break;
		case '%':
			w = emit_field(f, &sp, "", 0, 0, "%", 1);
			break;
		default: /* floating point */
			w = __fmt_fp(f, arg.f, sp.width, sp.prec,
				sp.fl | (sp.len == L_BIGL ? FL_LDBL : 0), sp.conv);
			break;
		}
		if (w == -2) { err = EILSEQ; goto fail; }
		if (w < 0 || w > INT_MAX - cnt) goto overflow;
		cnt += w;
	}
	return (int)cnt;
overflow:
	err = EOVERFLOW;
fail:
	errno = err;
	return -1;
}

hidden int __vfprintf_core(FILE *restrict f, const char *restrict fmt, va_list ap)
{
	va_list ap2;
	union arg pos_args[NL_MAX + 1];
	int r, err = 0;

	va_copy(ap2, ap);
	if (strchr(fmt, '$')) {
		int types[NL_MAX + 1] = { 0 };
		int p = collect_positional(fmt, types, &err);
		if (p < 0) {
			va_end(ap2);
			errno = err;
			return -1;
		}
		if (p) {
			int last = NL_MAX;
			while (last > 0 && !types[last]) last--;
			for (int i = 1; i <= last; i++) {
				if (!types[i]) {
					va_end(ap2);
					errno = EINVAL;
					return -1;
				}
				fetch(&pos_args[i], types[i], &ap2);
			}
		}
	}

	if (!f->mode) f->mode = -1;
	unsigned olderr = f->flags & F_ERR;
	f->flags &= ~F_ERR;
	r = format(f, fmt, &ap2, pos_args);
	if (f->flags & F_ERR) r = -1;
	f->flags |= olderr;
	va_end(ap2);
	return r;
}

int vfprintf(FILE *restrict f, const char *restrict fmt, va_list ap)
{
	int r;
	FLOCK(f);
	r = __vfprintf_core(f, fmt, ap);
	FUNLOCK(f);
	return r;
}
