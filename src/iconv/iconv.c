/*
 * lib-spfxd — iconv.
 *
 * Conversion goes through Unicode scalar values: the source is decoded
 * one character at a time and the character is encoded in the target.
 *
 * Supported: UTF-8, UTF-16 / UTF-16LE / UTF-16BE, UCS-2 / UCS-2LE /
 * UCS-2BE, UTF-32 / UTF-32LE / UTF-32BE, UCS-4 / UCS-4LE / UCS-4BE,
 * WCHAR_T, ASCII, ISO-8859-1, and the single-byte sets in iconv_tables.c
 * (ISO-8859-2..16, Windows-1250..1258, KOI8-R/U, CP437/850/866,
 * MacRoman).  "" and "CHAR" mean the locale's multibyte encoding.
 * Unicode encodings without an explicit byte order read a byte-order mark
 * if present (host order otherwise) and write one in host order, as glibc
 * does.  Multi-byte East Asian encodings are not provided:
 * iconv_open fails with EINVAL for them.
 *
 * Suffixes: //TRANSLIT replaces characters the target cannot represent
 * with '?'; //IGNORE drops them (and skips invalid input), after which
 * iconv reports EILSEQ once the remaining input has been converted.
 */
#include <errno.h>
#include <iconv.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "iconv_impl.h"
#include "locale_impl.h"

enum {
	C_UTF8, C_UTF16, C_UTF16LE, C_UTF16BE, C_UCS2, C_UCS2LE, C_UCS2BE,
	C_UTF32, C_UTF32LE, C_UTF32BE, C_UCS4LE, C_UCS4BE, C_ASCII, C_LATIN1, C_SBCS,
};

static const struct { const char *names; int code; } builtin[] = {
	{ "UTF8\0", C_UTF8 },
	{ "UTF16\0", C_UTF16 },
	{ "UTF16LE\0", C_UTF16LE },
	{ "UTF16BE\0", C_UTF16BE },
	{ "UCS2\0ISO10646UCS2\0CSUNICODE\0", C_UCS2 },
	{ "UCS2LE\0UNICODELITTLE\0", C_UCS2LE },
	{ "UCS2BE\0UNICODEBIG\0", C_UCS2BE },
	{ "UTF32\0", C_UTF32 },
	{ "UTF32LE\0", C_UTF32LE },
	{ "UTF32BE\0", C_UTF32BE },
	{ "UCS4\0ISO10646UCS4\0UCS4BE\0", C_UCS4BE },
	{ "UCS4LE\0WCHART\0", C_UCS4LE },
	{ "ASCII\0USASCII\0ANSIX3.41968\0ISO646US\0US\0", C_ASCII },
	{ "ISO88591\0LATIN1\0L1\0ISO885911987\0CP819\0IBM819\0", C_LATIN1 },
};

struct conv {
	unsigned char from, to;
	const uint16_t *from_tab, *to_tab;
	unsigned char translit, ignore;
	unsigned char in_order;          /* UTF-16/32 input: 0 unknown, 1 BE, 2 LE */
	unsigned char bom_done;          /* output BOM written */
};

/* Compare charset names ignoring case and the characters - _ space. */
static int name_eq(const char *a, const char *b, size_t blen)
{
	size_t i = 0;
	for (; *a && i < blen; ) {
		char ca = *a, cb = b[i];
		if (ca == '-' || ca == '_' || ca == ' ') { a++; continue; }
		if (cb == '-' || cb == '_' || cb == ' ') { i++; continue; }
		if ((ca | 32) != (cb | 32) && !(ca == cb)) return 0;
		a++;
		i++;
	}
	while (i < blen && (b[i] == '-' || b[i] == '_' || b[i] == ' ')) i++;
	while (*a == '-' || *a == '_' || *a == ' ') a++;
	return !*a && i == blen;
}

/* Resolve a charset name (with //suffixes) to a codec. */
static int find_codec(const char *name, const uint16_t **tab, int *translit, int *ignore)
{
	const char *sl = strstr(name, "//");
	size_t len = sl ? (size_t)(sl - name) : strlen(name);
	for (const char *s = sl; s && *s;) {
		s += 2;
		if (!strncasecmp(s, "TRANSLIT", 8)) translit && (*translit = 1);
		else if (!strncasecmp(s, "IGNORE", 6)) ignore && (*ignore = 1);
		s = strstr(s, "//");
	}
	*tab = 0;
	if (!len || name_eq("CHAR", name, len))
		return __locale_utf8() ? C_UTF8 : C_LATIN1;
	for (size_t i = 0; i < ARRAY_SIZE(builtin); i++)
		for (const char *n = builtin[i].names; *n; n += strlen(n) + 1)
			if (name_eq(n, name, len)) return builtin[i].code;
	for (const struct sbcs *c = __iconv_sbcs; c->name; c++) {
		int m = name_eq(c->name, name, len);
		for (const char *n = c->aliases; !m && *n; n += strlen(n) + 1) m = name_eq(n, name, len);
		if (m) {
			*tab = c->high;
			return C_SBCS;
		}
	}
	return -1;
}

iconv_t iconv_open(const char *to, const char *from)
{
	struct conv *c = calloc(1, sizeof *c);
	if (!c) return (iconv_t)-1;
	int tl = 0, ig = 0, ig2 = 0;
	int f = find_codec(from, &c->from_tab, 0, &ig2);
	int t = find_codec(to, &c->to_tab, &tl, &ig);
	if (f < 0 || t < 0) {
		free(c);
		errno = EINVAL;
		return (iconv_t)-1;
	}
	c->from = (unsigned char)f;
	c->to = (unsigned char)t;
	c->translit = (unsigned char)tl;
	c->ignore = (unsigned char)(ig | ig2);
	return c;
}

int iconv_close(iconv_t cd)
{
	free(cd);
	return 0;
}

static unsigned get16(const unsigned char *p, int le) { return le ? p[0] | p[1] << 8 : p[0] << 8 | p[1]; }
static uint32_t get32(const unsigned char *p, int le)
{
	return le ? (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24
	          : (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

/* Decode one character.  Returns bytes consumed (> 0), 0 for a consumed
 * byte-order mark (no character), -1 invalid (EILSEQ), -2 incomplete. */
static int decode(struct conv *c, const unsigned char *s, size_t n, uint32_t *wc)
{
	int le;
	switch (c->from) {
	case C_UTF8: {
		unsigned b = s[0];
		if (b < 0x80) { *wc = b; return 1; }
		int len;
		uint32_t v, min;
		if (b >= 0xc2 && b <= 0xdf) { len = 2; v = b & 0x1f; min = 0x80; }
		else if (b >= 0xe0 && b <= 0xef) { len = 3; v = b & 0x0f; min = 0x800; }
		else if (b >= 0xf0 && b <= 0xf4) { len = 4; v = b & 0x07; min = 0x10000; }
		else return -1;
		for (int k = 1; k < len; k++) {
			if ((size_t)k >= n) return -2;
			if ((s[k] & 0xc0) != 0x80) return -1;
			v = v << 6 | (s[k] & 0x3f);
			/* reject overlongs/surrogates/out of range as early as possible */
			if (k == 1 && ((b == 0xe0 && s[1] < 0xa0) || (b == 0xed && s[1] > 0x9f) ||
			               (b == 0xf0 && s[1] < 0x90) || (b == 0xf4 && s[1] > 0x8f)))
				return -1;
		}
		if (v < min) return -1;
		*wc = v;
		return len;
	}
	case C_UTF16: case C_UCS2:
		if (n < 2) return -2;
		if (!c->in_order) {
			if (s[0] == 0xfe && s[1] == 0xff) { c->in_order = 1; return 0; }
			if (s[0] == 0xff && s[1] == 0xfe) { c->in_order = 2; return 0; }
			c->in_order = 2;              /* no BOM: host order */
		}
		le = c->in_order == 2;
		goto u16;
	case C_UTF16LE: case C_UCS2LE: le = 1; goto u16;
	case C_UTF16BE: case C_UCS2BE: le = 0;
	u16: {
		if (n < 2) return -2;
		unsigned u = get16(s, le);
		int ucs2 = c->from == C_UCS2 || c->from == C_UCS2LE || c->from == C_UCS2BE;
		if (u >= 0xdc00 && u <= 0xdfff) return -1;
		if (u >= 0xd800 && u <= 0xdbff) {
			if (ucs2) return -1;
			if (n < 4) return -2;
			unsigned l = get16(s + 2, le);
			if (l < 0xdc00 || l > 0xdfff) return -1;
			*wc = 0x10000 + ((u - 0xd800) << 10) + (l - 0xdc00);
			return 4;
		}
		*wc = u;
		return 2;
	}
	case C_UTF32:
		if (n < 4) return -2;
		if (!c->in_order) {
			if (get32(s, 0) == 0xfeff) { c->in_order = 1; return 0; }
			if (get32(s, 1) == 0xfeff) { c->in_order = 2; return 0; }
			c->in_order = 2;
		}
		le = c->in_order == 2;
		goto u32;
	case C_UTF32LE: case C_UCS4LE: le = 1; goto u32;
	case C_UTF32BE: case C_UCS4BE: le = 0;
	u32: {
		if (n < 4) return -2;
		uint32_t v = get32(s, le);
		if (v > 0x10ffff || (v >= 0xd800 && v <= 0xdfff)) return -1;
		*wc = v;
		return 4;
	}
	case C_ASCII:
		if (s[0] >= 0x80) return -1;
		*wc = s[0];
		return 1;
	case C_LATIN1:
		*wc = s[0];
		return 1;
	case C_SBCS:
		if (s[0] < 0x80) { *wc = s[0]; return 1; }
		if (c->from_tab[s[0] - 0x80] == 0xfffd) return -1;
		*wc = c->from_tab[s[0] - 0x80];
		return 1;
	}
	return -1;
}

static void put16(unsigned char *p, unsigned v, int le)
{
	if (le) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
	else { p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v; }
}

static void put32(unsigned char *p, uint32_t v, int le)
{
	for (int i = 0; i < 4; i++) p[le ? i : 3 - i] = (unsigned char)(v >> (8 * i));
}

/* Encode wc.  Returns bytes written, -1 if not representable, -2 if
 * the output buffer is too small. */
static int encode(struct conv *c, uint32_t wc, unsigned char *o, size_t n)
{
	int le = 1, bom = 0;
	switch (c->to) {
	case C_UTF8:
		if (wc < 0x80) { if (n < 1) return -2; o[0] = (unsigned char)wc; return 1; }
		if (wc < 0x800) {
			if (n < 2) return -2;
			o[0] = (unsigned char)(0xc0 | wc >> 6);
			o[1] = (unsigned char)(0x80 | (wc & 0x3f));
			return 2;
		}
		if (wc < 0x10000) {
			if (n < 3) return -2;
			o[0] = (unsigned char)(0xe0 | wc >> 12);
			o[1] = (unsigned char)(0x80 | (wc >> 6 & 0x3f));
			o[2] = (unsigned char)(0x80 | (wc & 0x3f));
			return 3;
		}
		if (n < 4) return -2;
		o[0] = (unsigned char)(0xf0 | wc >> 18);
		o[1] = (unsigned char)(0x80 | (wc >> 12 & 0x3f));
		o[2] = (unsigned char)(0x80 | (wc >> 6 & 0x3f));
		o[3] = (unsigned char)(0x80 | (wc & 0x3f));
		return 4;
	case C_UTF16: case C_UCS2:
		bom = !c->bom_done;
		goto w16;
	case C_UTF16BE: case C_UCS2BE: le = 0; goto w16;
	case C_UTF16LE: case C_UCS2LE:
	w16: {
		int ucs2 = c->to == C_UCS2 || c->to == C_UCS2LE || c->to == C_UCS2BE;
		size_t need = (wc >= 0x10000 ? 4 : 2) + (bom ? 2 : 0);
		if (wc >= 0x10000 && ucs2) return -1;
		if (n < need) return -2;
		unsigned char *p = o;
		if (bom) {
			put16(p, 0xfeff, le);
			p += 2;
			c->bom_done = 1;
		}
		if (wc >= 0x10000) {
			put16(p, 0xd800 + ((wc - 0x10000) >> 10), le);
			put16(p + 2, 0xdc00 + ((wc - 0x10000) & 0x3ff), le);
		} else {
			put16(p, wc, le);
		}
		return (int)need;
	}
	case C_UTF32:
		bom = !c->bom_done;
		goto w32;
	case C_UTF32BE: case C_UCS4BE: le = 0; goto w32;
	case C_UTF32LE: case C_UCS4LE:
	w32: {
		size_t need = 4 + (bom ? 4 : 0);
		if (n < need) return -2;
		unsigned char *p = o;
		if (bom) {
			put32(p, 0xfeff, le);
			p += 4;
			c->bom_done = 1;
		}
		put32(p, wc, le);
		return (int)need;
	}
	case C_ASCII:
		if (wc >= 0x80) return -1;
		if (n < 1) return -2;
		o[0] = (unsigned char)wc;
		return 1;
	case C_LATIN1:
		if (wc >= 0x100) return -1;
		if (n < 1) return -2;
		o[0] = (unsigned char)wc;
		return 1;
	case C_SBCS:
		if (wc < 0x80) {
			if (n < 1) return -2;
			o[0] = (unsigned char)wc;
			return 1;
		}
		for (int i = 0; i < 128; i++) {
			if (c->to_tab[i] == wc && wc != 0xfffd) {
				if (n < 1) return -2;
				o[0] = (unsigned char)(0x80 + i);
				return 1;
			}
		}
		return -1;
	}
	return -1;
}

size_t iconv(iconv_t cd, char **restrict in, size_t *restrict inb, char **restrict out, size_t *restrict outb)
{
	struct conv *c = cd;
	size_t irrev = 0;
	int ignored = 0;
	if (!in || !*in) {
		/* reset: forget detected byte order and BOM state */
		c->in_order = 0;
		c->bom_done = 0;
		return 0;
	}
	const unsigned char *s = (const unsigned char *)*in;
	size_t n = *inb;
	while (n) {
		uint32_t wc;
		int l = decode(c, s, n, &wc);
		if (l == -2) {
			errno = EINVAL;
			goto fail;
		}
		if (l == -1) {
			if (c->ignore) {
				s++;
				n--;
				ignored = 1;
				continue;
			}
			errno = EILSEQ;
			goto fail;
		}
		if (!l) {                     /* byte-order mark */
			s += c->from == C_UTF32 ? 4 : 2;
			n -= c->from == C_UTF32 ? 4 : 2;
			continue;
		}
		if (!out || !*out) {
			errno = E2BIG;
			goto fail;
		}
		int w = encode(c, wc, (unsigned char *)*out, *outb);
		if (w == -1) {
			if (c->translit) {
				w = encode(c, '?', (unsigned char *)*out, *outb);
				irrev++;
			} else if (c->ignore) {
				ignored = 1;
				w = 0;
			} else {
				errno = EILSEQ;
				goto fail;
			}
		}
		if (w == -2) {
			errno = E2BIG;
			goto fail;
		}
		*out += w;
		*outb -= (size_t)w;
		s += l;
		n -= (size_t)l;
		*in = (char *)s;
		*inb = n;
	}
	*in = (char *)s;
	*inb = n;
	if (ignored) {
		errno = EILSEQ;
		return (size_t)-1;
	}
	return irrev;
fail:
	*in = (char *)s;
	*inb = n;
	return (size_t)-1;
}
