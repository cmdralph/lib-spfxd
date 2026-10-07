/*
 * lib-spfxd oracle test — POSIX regex.  Built against both the host C
 * library and lib-spfxd; the outputs must be identical.  Cases whose
 * sub-match assignment is ambiguous under POSIX are avoided; overall match
 * positions and error codes are compared everywhere.
 */
#include <locale.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>

struct tc { const char *re; int cf; const char *s; int ef; };
#define E REG_EXTENDED
#define I REG_ICASE
#define N REG_NEWLINE

static const struct tc cases[] = {
	{ "abc", 0, "xxabcxx", 0 }, { "a.c", 0, "abc", 0 }, { "a*", 0, "aaab", 0 },
	{ "a*b", 0, "caaab", 0 }, { "^abc", 0, "abc", 0 }, { "^abc", 0, "xabc", 0 },
	{ "abc$", 0, "xabc", 0 }, { "a\\(b\\)c", 0, "abc", 0 }, { "\\(a*\\)b", 0, "aab", 0 },
	{ "a\\{2\\}", 0, "aaa", 0 }, { "a\\{2,\\}", 0, "aaaaa", 0 }, { "a\\{1,3\\}", 0, "aaaaa", 0 },
	{ "\\(ab\\)\\1", 0, "abab", 0 }, { "\\(a\\)\\(b\\)\\2\\1", 0, "xabbay", 0 },
	{ "[abc]*", 0, "cabd", 0 }, { "[^abc]+", E, "abcdefa", 0 }, { "[a-z]+", E, "ABCdefGHI", 0 },
	{ "[[:digit:]]+", E, "abc123def", 0 }, { "[[:alpha:][:digit:]]+", E, "!!ab12!!", 0 },
	{ "[]a]+", E, "]a]x", 0 }, { "[^]a]+", E, "]axyz", 0 }, { "[a-]+", E, "a-a-b", 0 },
	{ "a|b|c", E, "xxc", 0 }, { "(a|ab)(c|bcd)", E, "abcd", 0 }, { "(a+)(b+)", E, "aaabbb", 0 },
	{ "(a*)*", E, "aaa", 0 }, { "(a*)+", E, "b", 0 }, { "(a|b)*c", E, "ababc", 0 },
	{ "x(a|b)?y", E, "xy", 0 }, { "x(a|b)?y", E, "xay", 0 }, { "a{3}", E, "aaaa", 0 },
	{ "a{2,3}", E, "aaaa", 0 }, { "a{,2}", E, "aaa", 0 }, { "(ab){2}", E, "ababab", 0 },
	{ "^$", E, "", 0 }, { "^", E, "abc", 0 }, { "$", E, "abc", 0 }, { "a$", E, "a\nb", 0 },
	{ "^b", E | N, "a\nb", 0 }, { "a$", E | N, "a\nb", 0 }, { "a.b", E | N, "a\nb", 0 },
	{ "a[^x]b", E | N, "a\nb", 0 }, { "a.b", E, "a\nb", 0 }, { "ABC", I, "xabcx", 0 },
	{ "[a-c]+", E | I, "xABCx", 0 }, { "(foo|bar)+", E, "foobarfoo", 0 },
	{ "\\bfoo\\b", E, "a foo b", 0 }, { "\\<foo", E, "xfoo foo", 0 }, { "foo\\>", E, "foox foo", 0 },
	{ "\\w+", E, "  hello_1 ", 0 }, { "\\W+", E, "ab  !c", 0 }, { "\\s+", E, "a \t b", 0 },
	{ "a+", E, "bbb", 0 }, { "", E, "abc", 0 }, { "()", E, "abc", 0 }, { "(a)|b", E, "b", 0 },
	{ "(a)(b)?", E, "a", 0 }, { "abc", 0, "abc", REG_NOTBOL }, { "^abc", 0, "abc", REG_NOTBOL },
	{ "abc$", 0, "abc", REG_NOTEOL }, { "a\\|b", 0, "b", 0 }, { "a\\+", 0, "baa", 0 },
	{ "a\\?b", 0, "ab", 0 }, { "*a", 0, "*a", 0 }, { "^*a", 0, "*a", 0 }, { "a**", 0, "aaa", 0 },
	{ "\\(^a\\)", 0, "a", 0 }, { "a\\{,2\\}", 0, "aaa", 0 },
	{ "(a|b)\\1", E, "aa", 0 }, { "(a|b)\\1", E, "ab", 0 }, { "(.)\\1+", E, "abbbc", 0 },
	{ "x*(.)\\1", E, "xxyy", 0 },
	/* errors */
	{ "a\\{1", 0, "", 0 }, { "a(", E, "", 0 }, { "a)", E, "a)", 0 }, { "[a", E, "", 0 },
	{ "a\\", 0, "", 0 }, { "[[:foo:]]", E, "", 0 }, { "\\1", 0, "", 0 }, { "a{2,1}", E, "", 0 },
	{ "[z-a]", E, "", 0 }, { "*a", E, "", 0 }, { "a{1", E, "", 0 }, { "\\(a", 0, "", 0 },
	{ "a\\)", 0, "", 0 }, { "(a|)+", E, "aa", 0 }, { "a{256}", E, "", 0 },
	/* longer subjects */
	{ "[0-9]+\\.[0-9]+", E, "version 12.345 and 6.7", 0 },
	{ "([a-z]+)@([a-z]+)\\.com", E, "mail bob@example.com now", 0 },
	{ "(ab|a)(bc|c)?", E, "abc", 0 },
};

int main(void)
{
	setlocale(LC_ALL, "C");
	for (size_t k = 0; k < sizeof cases / sizeof *cases; k++) {
		const struct tc *t = &cases[k];
		regex_t re;
		int r = regcomp(&re, t->re, t->cf);
		printf("%zu /%s/%d: ", k, t->re, t->cf);
		if (r) {
			char buf[128];
			regerror(r, &re, buf, sizeof buf);
			printf("compile error %d\n", r);
			continue;
		}
		regmatch_t m[10];
		r = regexec(&re, t->s, 10, m, t->ef);
		if (r) {
			printf("nomatch %d\n", r);
		} else {
			printf("nsub=%zu", re.re_nsub);
			for (size_t i = 0; i <= re.re_nsub && i < 10; i++) printf(" (%ld,%ld)", (long)m[i].rm_so, (long)m[i].rm_eo);
			printf("\n");
		}
		regfree(&re);
	}
	/* REG_STARTEND and REG_NOSUB */
	regex_t re;
	regcomp(&re, "b+", REG_EXTENDED);
	regmatch_t m[1] = { { 3, 7 } };
	printf("startend %d (%ld,%ld)\n", regexec(&re, "abbabbbx", 1, m, REG_STARTEND), (long)m[0].rm_so, (long)m[0].rm_eo);
	regfree(&re);
	regcomp(&re, "(a)(b)", REG_EXTENDED | REG_NOSUB);
	printf("nosub %d\n", regexec(&re, "xab", 0, 0, 0));
	regfree(&re);
	return 0;
}
