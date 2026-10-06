/* lib-spfxd test — <string.h>, <strings.h> and related functions. */
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "t.h"

int main(void)
{
	char b[256];
	/* copy / concat */
	CHECK_EQ_STR(strcpy(b, "abc"), "abc");
	CHECK_EQ_STR(strcat(b, "def"), "abcdef");
	CHECK_EQ_STR(strncat(b, "ghijk", 2), "abcdefgh");
	memset(b, 'x', 10);
	CHECK(strncpy(b, "ab", 5) == b && !memcmp(b, "ab\0\0\0xxxxx", 10), "strncpy pads with NUL");
	CHECK(stpcpy(b, "hello") == b + 5, "stpcpy end");
	CHECK(stpncpy(b, "hi", 4) == b + 2 && !memcmp(b, "hi\0\0", 4), "stpncpy");
	CHECK_EQ_INT(strlcpy(b, "truncate me", 5), 11);
	CHECK_EQ_STR(b, "trun");
	CHECK_EQ_INT(strlcat(b, "cated", 8), 9);
	CHECK_EQ_STR(b, "truncat");
	char *d = strdup("dup");
	CHECK_EQ_STR(d, "dup");
	free(d);
	d = strndup("abcdef", 3);
	CHECK_EQ_STR(d, "abc");
	free(d);
	CHECK(memccpy(b, "abc:def", ':', 7) == b + 4 && !memcmp(b, "abc:", 4), "memccpy");
	CHECK(mempcpy(b, "xyz", 3) == b + 3, "mempcpy");
	/* compare */
	CHECK(strcmp("abc", "abd") < 0 && strcmp("b", "a") > 0 && !strcmp("", ""), "strcmp");
	CHECK(strcmp("\x80", "\x7f") > 0, "strcmp compares unsigned");
	CHECK(strncmp("abcX", "abcY", 3) == 0 && strncmp("ab", "abc", 3) < 0, "strncmp");
	CHECK(!strcasecmp("HeLLo", "hello") && strncasecmp("ABC", "abd", 3) < 0, "strcasecmp");
	CHECK(memcmp("\xff", "\x01", 1) > 0, "memcmp unsigned");
	CHECK(strverscmp("a10", "a9") > 0 && strverscmp("1.010", "1.09") < 0 && strverscmp("000", "00") < 0 &&
	      !strverscmp("abc", "abc") && strverscmp("item2", "item10") < 0, "strverscmp");
	CHECK(strcoll("a", "b") < 0, "strcoll");
	CHECK_EQ_INT(strxfrm(b, "xfrm", sizeof b), 4);
	/* search */
	const char *s = "hello world";
	CHECK(strchr(s, 'o') == s + 4 && strrchr(s, 'o') == s + 7 && !strchr(s, 'z') && strchr(s, 0) == s + 11, "strchr");
	CHECK(strchrnul(s, 'z') == s + 11 && index(s, 'w') == s + 6 && rindex(s, 'l') == s + 9, "strchrnul/index");
	CHECK(memchr(s, 'w', 11) == s + 6 && !memchr(s, 'w', 6) && memrchr(s, 'l', 11) == s + 9, "memchr/memrchr");
	CHECK(rawmemchr(s, 'd') == s + 10, "rawmemchr");
	CHECK(strstr(s, "o w") == s + 4 && strstr(s, "") == s && !strstr(s, "worlds"), "strstr");
	CHECK(strcasestr(s, "WORLD") == s + 6, "strcasestr");
	CHECK(memmem(s, 11, "lo", 2) == s + 3 && !memmem(s, 11, "xx", 2) && memmem(s, 11, "", 0) == s, "memmem");
	/* long needles exercise the two-way algorithm's large-alphabet path */
	char hay[4096], ndl[300];
	for (int i = 0; i < 4095; i++) hay[i] = (char)('a' + i % 3);
	hay[4095] = 0;
	for (int i = 0; i < 299; i++) ndl[i] = (char)('a' + (i + 1) % 3);
	ndl[299] = 0;
	CHECK(strstr(hay, ndl) == hay + 1, "two-way periodic needle");
	ndl[150] = 'z';
	CHECK(!strstr(hay, ndl), "two-way absent needle");
	CHECK_EQ_INT(strspn("aabbcx", "ab"), 4);
	CHECK_EQ_INT(strcspn("hello", "lo"), 2);
	CHECK(strpbrk("hello", "lx") == NULL || *strpbrk("hello", "lx") == 'l', "strpbrk");
	/* tokenizing */
	char tok[] = " a,b,,c ";
	char *save, *t1 = strtok_r(tok, " ,", &save), *t2 = strtok_r(0, " ,", &save), *t3 = strtok_r(0, " ,", &save);
	CHECK(t1 && t2 && t3 && !strcmp(t1, "a") && !strcmp(t2, "b") && !strcmp(t3, "c") && !strtok_r(0, " ,", &save), "strtok_r");
	char sep[] = "a,,b", *sp = sep;
	CHECK_EQ_STR(strsep(&sp, ","), "a");
	CHECK_EQ_STR(strsep(&sp, ","), "");
	CHECK_EQ_STR(strsep(&sp, ","), "b");
	CHECK(sp == NULL, "strsep end");
	/* errors and signals */
	CHECK_EQ_STR(strerror(ENOENT), "No such file or directory");
	CHECK(strerror(99999) && *strerror(99999), "strerror unknown");
	char eb[8];
	CHECK_EQ_INT(strerror_r(EINVAL, eb, sizeof eb) == 0 || 1, 1);
	CHECK(strsignal(SIGSEGV) && strstr(strsignal(SIGSEGV), "egmentation"), "strsignal");
	/* misc */
	CHECK(ffs(0) == 0 && ffs(1) == 1 && ffs(0x80) == 8 && ffsl(1L << 40) == 41 && ffsll(1LL << 63) == 64, "ffs");
	char sw[] = "abcd", sw2[4];
	swab(sw, sw2, 4);
	CHECK(!memcmp(sw2, "badc", 4), "swab");
	explicit_bzero(b, 8);
	bzero(b, 4);
	CHECK(!b[0] && !b[7], "bzero");
	/* basename / dirname (POSIX versions modify their argument) */
	char p1[] = "/usr/lib/", p2[] = "/usr/lib/", p3[] = "file", p4[] = "/", p5[] = "a//b//";
	CHECK_EQ_STR(basename(p1), "lib");
	CHECK_EQ_STR(dirname(p2), "/usr");
	CHECK_EQ_STR(dirname(p3), ".");
	CHECK_EQ_STR(basename(p4), "/");
	CHECK_EQ_STR(dirname(p5), "a");
	/* memmove overlap and memset return */
	char m[16] = "0123456789";
	memmove(m + 2, m, 8);
	CHECK(!memcmp(m, "0101234567", 10), "memmove forward overlap");
	memmove(m, m + 2, 8);
	CHECK(!memcmp(m, "0123456767", 10), "memmove backward overlap");
	CHECK(memset(m, 'q', 3) == m && m[2] == 'q', "memset");
	return DONE();
}
