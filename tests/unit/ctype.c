/* lib-spfxd test — <ctype.h> and <wctype.h> in the C and UTF-8 locales. */
#include <ctype.h>
#include <locale.h>
#include <wchar.h>
#include <wctype.h>
#include "t.h"

int main(void)
{
	/* C locale: exactly the ASCII classes, nothing above 0x7f */
	for (int c = -1; c < 256; c++) {
		int ascii = c >= 0 && c < 128;
		CHECK(!!isalpha(c) == (ascii && ((c | 32) >= 'a' && (c | 32) <= 'z')), "isalpha %d", c);
		CHECK(!!isdigit(c) == (c >= '0' && c <= '9'), "isdigit %d", c);
		CHECK(!!isxdigit(c) == (isdigit(c) || ((c | 32) >= 'a' && (c | 32) <= 'f')), "isxdigit %d", c);
		CHECK(!!isspace(c) == (c == ' ' || (c >= 9 && c <= 13)), "isspace %d", c);
		CHECK(!!isblank(c) == (c == ' ' || c == '\t'), "isblank %d", c);
		CHECK(!!iscntrl(c) == ((c >= 0 && c < 32) || c == 127), "iscntrl %d", c);
		CHECK(!!isprint(c) == (c >= 32 && c < 127), "isprint %d", c);
		CHECK(!!isgraph(c) == (c > 32 && c < 127), "isgraph %d", c);
		CHECK(!!ispunct(c) == (isgraph(c) && !isalnum(c)), "ispunct %d", c);
		CHECK(!!isupper(c) == (c >= 'A' && c <= 'Z'), "isupper %d", c);
		CHECK(!!islower(c) == (c >= 'a' && c <= 'z'), "islower %d", c);
		CHECK(toupper(c) == (islower(c) ? c - 32 : c), "toupper %d", c);
		CHECK(tolower(c) == (isupper(c) ? c + 32 : c), "tolower %d", c);
	}
	CHECK(isascii(0x7f) && !isascii(0x80) && toascii(0x1ff) == 0x7f, "isascii/toascii");
	/* wide classification is Unicode-based in every locale (documented) */
	CHECK(iswalpha(0xe9) && towupper(0xe9) == 0xc9, "wide classes are Unicode");

	/* UTF-8 locale: Unicode classes */
	CHECK(setlocale(LC_CTYPE, "C.UTF-8") != NULL, "C.UTF-8 available");
	CHECK(iswalpha(0xe9) && iswalpha(0x3b1) && iswalpha(0x4e2d) && !iswalpha(0x2013), "iswalpha");
	CHECK(iswupper(0xc9) && iswlower(0xe9) && towupper(0xe9) == 0xc9 && towlower(0x391) == 0x3b1, "case mapping");
	CHECK(towupper(0x1f600) == 0x1f600 && towlower(L'Z') == L'z', "case mapping identity");
	CHECK(iswdigit(L'7') && !iswdigit(0x664), "iswdigit is ASCII only");
	CHECK(iswspace(0x2003) && iswspace(0x3000) && !iswspace(0xa0), "iswspace (NBSP is not space)");
	CHECK(iswpunct(0x2014) && iswpunct(L'!') && !iswpunct(L'a'), "iswpunct");
	CHECK(iswcntrl(0x85) && iswcntrl(0x2028) && !iswcntrl(L'a'), "iswcntrl");
	CHECK(iswprint(0x4e2d) && !iswprint(0x7f) && !iswprint(0xd800), "iswprint");
	CHECK(wcwidth(L'a') == 1 && wcwidth(0x4e2d) == 2 && wcwidth(0x301) == 0 && wcwidth(0) == 0 &&
	      wcwidth(1) == -1 && wcwidth(0x1f600) == 2, "wcwidth");
	CHECK(wcswidth(L"a\x4e2dz", 3) == 4, "wcswidth");
	wctype_t al = wctype("alpha"), bad = wctype("nonsense");
	CHECK(al && !bad && iswctype(0x416, al), "wctype/iswctype");
	wctrans_t up = wctrans("toupper");
	CHECK(up && towctrans(0x3c9, up) == 0x3a9 && !wctrans("x"), "wctrans");
	/* byte classification stays ASCII in UTF-8 locales */
	CHECK(!isalpha(0xe9), "isalpha byte in UTF-8 locale");
	return DONE();
}
