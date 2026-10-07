/* lib-spfxd test — locales, multibyte/wide conversion (exhaustively over
 * all code points), wide strings, uchar.h, langinfo. */
#include <errno.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <uchar.h>
#include <wchar.h>
#include "t.h"

int main(void)
{
	CHECK(!strcmp(setlocale(LC_ALL, 0), "C"), "initial locale C");
	CHECK(setlocale(LC_ALL, "POSIX") && setlocale(LC_ALL, "") , "POSIX and \"\" accepted");
	CHECK(!setlocale(LC_ALL, "xx_YY.FAKE"), "unknown locale rejected");
	struct lconv *lc = localeconv();
	CHECK(!strcmp(lc->decimal_point, ".") && !*lc->thousands_sep && lc->int_frac_digits == CHAR_MAX, "localeconv C");
	setlocale(LC_ALL, "C");
	CHECK(MB_CUR_MAX == 1, "MB_CUR_MAX in C");
	/* C locale: every byte round-trips */
	for (int b = 1; b < 256; b++) {
		char c = (char)b;
		wchar_t w;
		mbstate_t st = { 0 };
		CHECK(mbrtowc(&w, &c, 1, &st) == 1 && w == (wchar_t)b, "C mbrtowc byte %d", b);
		char out[4];
		CHECK(wcrtomb(out, w, &st) == 1 && (unsigned char)out[0] == b, "C wcrtomb byte %d", b);
	}
	CHECK(!strcmp(nl_langinfo(CODESET), "ANSI_X3.4-1968") || strlen(nl_langinfo(CODESET)), "CODESET");

	CHECK(setlocale(LC_CTYPE, "en_US.UTF-8") != NULL, "UTF-8 locale");
	CHECK(MB_CUR_MAX == 4 && !strcmp(nl_langinfo(CODESET), "UTF-8"), "UTF-8 MB_CUR_MAX/CODESET");
	/* exhaustive round trip of every Unicode scalar value */
	int bad = 0;
	for (unsigned u = 1; u <= 0x10ffff && bad < 5; u++) {
		if (u >= 0xd800 && u <= 0xdfff) {
			char tmp[8];
			mbstate_t st = { 0 };
			errno = 0;
			if (wcrtomb(tmp, (wchar_t)u, &st) != (size_t)-1 || errno != EILSEQ) bad++;
			continue;
		}
		char buf[8];
		mbstate_t st = { 0 };
		size_t n = wcrtomb(buf, (wchar_t)u, &st);
		wchar_t w = 0;
		if (n == (size_t)-1 || n != (u < 0x80 ? 1u : u < 0x800 ? 2u : u < 0x10000 ? 3u : 4u) ||
		    mbrtowc(&w, buf, n, &st) != n || (unsigned)w != u)
			bad++;
	}
	CHECK(!bad, "UTF-8 round trip of all scalar values (%d bad)", bad);
	/* invalid sequences */
	static const char *const invalid[] = {
		"\x80", "\xc0\x80", "\xc1\xbf", "\xe0\x80\x80", "\xed\xa0\x80", "\xf0\x80\x80\x80",
		"\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xff", "\xc3\x28", "\xe2\x28\xa1",
	};
	for (size_t i = 0; i < sizeof invalid / sizeof *invalid; i++) {
		wchar_t w;
		mbstate_t st = { 0 };
		errno = 0;
		CHECK(mbrtowc(&w, invalid[i], strlen(invalid[i]), &st) == (size_t)-1 && errno == EILSEQ, "invalid #%zu", i);
	}
	/* incomplete input continues across calls */
	mbstate_t st = { 0 };
	wchar_t w;
	CHECK(mbrtowc(&w, "\xe4", 1, &st) == (size_t)-2 && mbrtowc(&w, "\xb8", 1, &st) == (size_t)-2 &&
	      mbrtowc(&w, "\xad", 1, &st) == 1 && w == 0x4e2d, "incremental decode");
	CHECK(mbrlen("\xf0\x9f\x98\x80", 4, 0) == 4 && mblen("\xc3\xa9", 2) == 2 && mblen(0, 0) == 0, "mbrlen/mblen");
	CHECK(mbsinit(&st), "mbsinit");
	/* string conversions */
	wchar_t ws[32];
	CHECK(mbstowcs(ws, "h\xc3\xa9llo \xe4\xb8\xad", 32) == 7 && ws[1] == 0xe9 && ws[6] == 0x4e2d, "mbstowcs");
	CHECK(mbstowcs(0, "h\xc3\xa9", 0) == 2, "mbstowcs length query");
	char mb[64];
	CHECK(wcstombs(mb, L"é中", sizeof mb) == 5 && !strcmp(mb, "\xc3\xa9\xe4\xb8\xad"), "wcstombs");
	CHECK(wcstombs(mb, L"\xd800", sizeof mb) == (size_t)-1, "wcstombs surrogate");
	const char *src = "abc\xc3\xa9";
	memset(&st, 0, sizeof st);
	CHECK(mbsnrtowcs(ws, &src, 3, 32, &st) == 3 && src && *src == '\xc3', "mbsnrtowcs partial");
	/* uchar.h */
	char16_t c16;
	memset(&st, 0, sizeof st);
	CHECK(mbrtoc16(&c16, "\xf0\x9f\x98\x80", 4, &st) == 4 && c16 == 0xd83d, "mbrtoc16 high surrogate");
	CHECK(mbrtoc16(&c16, "", 0, &st) == (size_t)-3 && c16 == 0xde00, "mbrtoc16 low surrogate");
	memset(&st, 0, sizeof st);
	CHECK(c16rtomb(mb, 0xd83d, &st) == 0 && c16rtomb(mb, 0xde00, &st) == 4 && !memcmp(mb, "\xf0\x9f\x98\x80", 4), "c16rtomb pair");
	char32_t c32;
	memset(&st, 0, sizeof st);
	CHECK(mbrtoc32(&c32, "\xe2\x82\xac", 3, &st) == 3 && c32 == 0x20ac && c32rtomb(mb, 0x1f600, &st) == 4, "c32");
	/* wide string functions */
	CHECK(wcslen(L"abc") == 3 && !wcscmp(wcscpy(ws, L"xyz"), L"xyz") && wcschr(L"abc", L'b') && wcsstr(L"hello", L"ll"), "wcs basics");
	CHECK(wcstol(L"  -42z", 0, 10) == -42 && wcstod(L"2.5e1", 0) == 25.0 && wcstoull(L"ff", 0, 16) == 255, "wcsto*");
	CHECK(wcsncmp(L"abcd", L"abce", 3) == 0 && wcscasecmp(L"ABC", L"abc") == 0 && wmemcmp(L"a", L"b", 1) < 0, "wcs compare");
	CHECK(swprintf(ws, 32, L"%d %ls %.2f", 7, L"w", 1.5) == 8 && !wcscmp(ws, L"7 w 1.50"), "swprintf");
	CHECK(swprintf(ws, 3, L"%s", "toolong") == -1, "swprintf overflow");
	CHECK(wcsdup(L"dup") && wcsnlen(L"abc", 2) == 2 && wcsspn(L"aab", L"a") == 2 && wcspbrk(L"abc", L"c"), "wcsdup etc");
	wchar_t tk[] = L"a b", *sv, *t1 = wcstok(tk, L" ", &sv);
	CHECK(t1 && !wcscmp(t1, L"a") && !wcscmp(wcstok(0, L" ", &sv), L"b"), "wcstok");
	CHECK(wcscoll(L"a", L"b") < 0 && wcsxfrm(ws, L"q", 32) == 1, "wcscoll");
	CHECK(btowc('A') == L'A' && btowc(0xc3) == WEOF && wctob(0xe9) == EOF && wctob(L'z') == 'z', "btowc/wctob");
	/* newlocale / uselocale */
	locale_t l = newlocale(LC_CTYPE_MASK, "C.UTF-8", 0);
	CHECK(l != 0, "newlocale");
	setlocale(LC_ALL, "C");
	locale_t old = uselocale(l);
	CHECK(MB_CUR_MAX == 4, "uselocale switches thread locale");
	uselocale(old);
	CHECK(MB_CUR_MAX == 1, "uselocale restored");
	locale_t d = duplocale(l);
	CHECK(d != 0, "duplocale");
	freelocale(d);
	freelocale(l);
	return DONE();
}
