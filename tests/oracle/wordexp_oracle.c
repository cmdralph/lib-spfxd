/* lib-spfxd oracle test — wordexp */
#include <stdio.h>
#include <stdlib.h>
#include <wordexp.h>
static void t(const char *s, int f)
{
	wordexp_t w;
	int r = wordexp(s, &w, f);
	printf("[%s] %#x -> %d", s, f, r);
	if (!r) { for (size_t i = 0; i < w.we_wordc; i++) printf(" <%s>", w.we_wordv[i]); wordfree(&w); }
	printf("\n");
}
int main(void)
{
	setenv("WX", "a b  c", 1);
	setenv("HOME", "/home/test", 1);
	t("hello world", 0); t("'a b' \"c d\" e\\ f", 0); t("$WX", 0); t("\"$WX\"", 0);
	t("${WX}x", 0); t("~/x", 0); t("$((1+2))", 0); t("$(echo hi there)", 0); t("`echo q`", 0);
	t("$(echo hi)", WRDE_NOCMD); t("`echo q`", WRDE_NOCMD); t("a|b", 0); t("a;b", 0); t("a>b", 0);
	t("'unterminated", 0); t("$UNDEFINED_VAR_X", 0); t("$UNDEFINED_VAR_X", WRDE_UNDEF);
	t("", 0); t("   ", 0); t("\"a|b\"", 0); t("a\\|b", 0); t("x{y}", 0); t("${WX:-d}", 0);
	wordexp_t w;
	w.we_offs = 2;
	wordexp("one two", &w, WRDE_DOOFFS);
	wordexp("three", &w, WRDE_DOOFFS | WRDE_APPEND);
	printf("offs: %zu %s %s %s %s %s\n", w.we_wordc, w.we_wordv[0] ? "?" : "NULL", w.we_wordv[1] ? "?" : "NULL",
	       w.we_wordv[2], w.we_wordv[3], w.we_wordv[4]);
	wordfree(&w);
	return 0;
}
