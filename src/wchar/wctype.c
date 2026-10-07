/*
 * lib-spfxd — <wctype.h>.  Classification follows the Unicode tables in
 * src/ctype/unicode_data.c (see tools/gen-unicode.py for the definitions);
 * digits and hex digits are ASCII only, as ISO C requires.
 */
#include <wctype.h>
#include <string.h>
#include <stdint.h>
#include "libc.h"


#define VALID(c) ((c) < 0x110000)

int iswalpha(wint_t c) { return VALID(c) && __uni_alpha(c); }
int iswdigit(wint_t c) { return c - '0' < 10; }
int iswalnum(wint_t c) { return iswdigit(c) || iswalpha(c); }
int iswblank(wint_t c) { return c == ' ' || c == '\t' || c == 0x1680 || (c - 0x2000 < 11 && c != 0x2007) || c == 0x205f || c == 0x3000; }
int iswcntrl(wint_t c) { return VALID(c) && __uni_cntrl(c); }
int iswgraph(wint_t c) { return VALID(c) && __uni_graph(c); }
int iswlower(wint_t c) { return VALID(c) && __uni_lower(c); }
int iswupper(wint_t c) { return VALID(c) && __uni_upper(c); }
int iswprint(wint_t c) { return VALID(c) && __uni_print(c); }
int iswpunct(wint_t c) { return VALID(c) && __uni_punct(c); }
int iswspace(wint_t c) { return VALID(c) && __uni_space(c); }
int iswxdigit(wint_t c) { return c - '0' < 10 || (c | 32) - 'a' < 6; }
wint_t towlower(wint_t c) { return VALID(c) ? __uni_tolower(c) : c; }
wint_t towupper(wint_t c) { return VALID(c) ? __uni_toupper(c) : c; }

enum { WC_ALNUM = 1, WC_ALPHA, WC_BLANK, WC_CNTRL, WC_DIGIT, WC_GRAPH, WC_LOWER,
	WC_PRINT, WC_PUNCT, WC_SPACE, WC_UPPER, WC_XDIGIT };
static const char class_names[] =
	"alnum\0alpha\0blank\0cntrl\0digit\0graph\0lower\0print\0punct\0space\0upper\0xdigit\0";

wctype_t wctype(const char *name)
{
	const char *p = class_names;
	for (wctype_t i = 1; *p; i++, p += strlen(p) + 1)
		if (!strcmp(name, p)) return i;
	return 0;
}

int iswctype(wint_t c, wctype_t t)
{
	switch (t) {
	case WC_ALNUM: return iswalnum(c);
	case WC_ALPHA: return iswalpha(c);
	case WC_BLANK: return iswblank(c);
	case WC_CNTRL: return iswcntrl(c);
	case WC_DIGIT: return iswdigit(c);
	case WC_GRAPH: return iswgraph(c);
	case WC_LOWER: return iswlower(c);
	case WC_PRINT: return iswprint(c);
	case WC_PUNCT: return iswpunct(c);
	case WC_SPACE: return iswspace(c);
	case WC_UPPER: return iswupper(c);
	case WC_XDIGIT: return iswxdigit(c);
	}
	return 0;
}

static const int trans_upper = 1, trans_lower = 2;

wctrans_t wctrans(const char *name)
{
	if (!strcmp(name, "toupper")) return &trans_upper;
	if (!strcmp(name, "tolower")) return &trans_lower;
	return 0;
}

wint_t towctrans(wint_t c, wctrans_t t)
{
	if (t == &trans_upper) return towupper(c);
	if (t == &trans_lower) return towlower(c);
	return c;
}

int iswalnum_l(wint_t c, locale_t l) { return iswalnum(c); }
int iswalpha_l(wint_t c, locale_t l) { return iswalpha(c); }
int iswblank_l(wint_t c, locale_t l) { return iswblank(c); }
int iswcntrl_l(wint_t c, locale_t l) { return iswcntrl(c); }
int iswdigit_l(wint_t c, locale_t l) { return iswdigit(c); }
int iswgraph_l(wint_t c, locale_t l) { return iswgraph(c); }
int iswlower_l(wint_t c, locale_t l) { return iswlower(c); }
int iswprint_l(wint_t c, locale_t l) { return iswprint(c); }
int iswpunct_l(wint_t c, locale_t l) { return iswpunct(c); }
int iswspace_l(wint_t c, locale_t l) { return iswspace(c); }
int iswupper_l(wint_t c, locale_t l) { return iswupper(c); }
int iswxdigit_l(wint_t c, locale_t l) { return iswxdigit(c); }
int iswctype_l(wint_t c, wctype_t t, locale_t l) { return iswctype(c, t); }
wint_t towlower_l(wint_t c, locale_t l) { return towlower(c); }
wint_t towupper_l(wint_t c, locale_t l) { return towupper(c); }
wctype_t wctype_l(const char *s, locale_t l) { return wctype(s); }
wint_t towctrans_l(wint_t c, wctrans_t t, locale_t l) { return towctrans(c, t); }
wctrans_t wctrans_l(const char *s, locale_t l) { return wctrans(s); }
