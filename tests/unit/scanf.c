/* lib-spfxd test — scanf family. */
#include <stdio.h>
#include <wchar.h>
#include <locale.h>
#include "t.h"

int main(void)
{
	int a, b, n;
	char s[32], s2[32];
	double d;
	float f;
	long double ld;
	unsigned u;
	long long ll;
	CHECK(sscanf("12 34", "%d %d", &a, &b) == 2 && a == 12 && b == 34, "two ints");
	CHECK(sscanf("  -17abc", "%d%s", &a, s) == 2 && a == -17 && !strcmp(s, "abc"), "int then string");
	CHECK(sscanf("0x1F 017 10", "%i %i %i", &a, &b, &n) == 3 && a == 31 && b == 15 && n == 10, "%%i bases");
	CHECK(sscanf("ff", "%x", &u) == 1 && u == 255 && sscanf("777", "%o", &u) == 1 && u == 511, "%%x %%o");
	CHECK(sscanf("3.25e2 1.5 2.5", "%lf %f %Lf", &d, &f, &ld) == 3 && d == 325 && f == 1.5f && ld == 2.5L, "floats");
	CHECK(sscanf("0x1p-2 inf nan", "%lf %f %lf", &d, &f, &d) == 3 && f > 1e30f && d != d, "hex float, inf, nan");
	CHECK(sscanf("abc", "%d", &a) == 0, "matching failure");
	CHECK(sscanf("", "%d", &a) == EOF, "input failure at start");
	CHECK(sscanf("hello world", "%5s%n", s, &n) == 1 && !strcmp(s, "hello") && n == 5, "width and %%n");
	CHECK(sscanf("abc123", "%[a-z]%[0-9]", s, s2) == 2 && !strcmp(s, "abc") && !strcmp(s2, "123"), "scansets");
	CHECK(sscanf("x]y", "%[]x]", s) == 1 && !strcmp(s, "x]"), "scanset with ]");
	CHECK(sscanf("abc-def", "%[^-]-%s", s, s2) == 2 && !strcmp(s2, "def"), "negated scanset");
	CHECK(sscanf("12345", "%3d%d", &a, &b) == 2 && a == 123 && b == 45, "int width");
	CHECK(sscanf("abc", "%c%c", s, s + 1) == 2 && s[0] == 'a' && s[1] == 'b', "%%c");
	CHECK(sscanf("10:20", "%d:%d", &a, &b) == 2 && b == 20, "literal");
	CHECK(sscanf("10 % 20", "%d %% %d", &a, &b) == 2 && b == 20, "%%%% literal");
	CHECK(sscanf("1 2 3", "%*d %d", &a) == 1 && a == 2, "assignment suppression");
	CHECK(sscanf("9223372036854775807", "%lld", &ll) == 1 && ll == 9223372036854775807LL, "%%lld");
	char *m = 0;
	CHECK(sscanf("dynamic string", "%ms", &m) == 1 && m && !strcmp(m, "dynamic"), "%%ms");
	free(m);
	CHECK(sscanf("3 4", "%2$d %1$d", &a, &b) == 2 && a == 4 && b == 3, "positional");
	short h;
	signed char hh;
	CHECK(sscanf("70000 300", "%hd %hhd", &h, &hh) == 2 && h == (short)70000 && hh == (signed char)300, "%%hd %%hhd");
	void *p;
	CHECK(sscanf("0x1234", "%p", &p) == 1 && p == (void *)0x1234, "%%p");
	/* C11 7.21.6.2p9: "1.5e" is a prefix of a numeral but not one */
	CHECK(sscanf("1.5e", "%lf%s", &d, s) == 0, "dangling exponent is a matching failure");
	CHECK(sscanf("100ergs", "%lf%s", &d, s) == 0, "the standard's 100ergs example");
	CHECK(sscanf("1.5 e", "%lf%s", &d, s) == 2 && d == 1.5, "complete numeral then string");
	CHECK(sscanf("-", "%d", &a) == 0, "lone sign");
	CHECK(sscanf("  \n\t42", "%d", &a) == 1 && a == 42, "leading whitespace");
	/* stream scanning with pushback */
	FILE *t = tmpfile();
	fputs("100 200 word\n", t);
	rewind(t);
	CHECK(fscanf(t, "%d", &a) == 1 && fscanf(t, "%d %s", &b, s) == 2 && b == 200 && !strcmp(s, "word"), "fscanf");
	CHECK(fscanf(t, "%d", &a) == EOF, "fscanf EOF");
	fclose(t);
	/* wide */
	setlocale(LC_CTYPE, "C.UTF-8");
	wchar_t ws[16];
	CHECK(swscanf(L"42 中文", L"%d %ls", &a, ws) == 2 && a == 42 && !wcscmp(ws, L"中文"), "swscanf");
	CHECK(sscanf("\xe4\xb8\xad x", "%ls", ws) == 1 && ws[0] == 0x4e2d && !ws[1], "%%ls converts multibyte");
	return DONE();
}
