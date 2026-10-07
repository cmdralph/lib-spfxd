/* lib-spfxd — strcasecmp / strncasecmp (+ _l): ASCII case folding, which
 * is exact for the C locale and for UTF-8 encoded text. */
#include <strings.h>
#include <ctype.h>

static __inline int fold(unsigned char c)
{
	return c - 'A' < 26u ? c | 0x20 : c;
}

int strcasecmp(const char *l, const char *r)
{
	const unsigned char *a = (const void *)l, *b = (const void *)r;
	for (; *a && (*a == *b || fold(*a) == fold(*b)); a++, b++);
	return fold(*a) - fold(*b);
}

int strncasecmp(const char *l, const char *r, size_t n)
{
	const unsigned char *a = (const void *)l, *b = (const void *)r;
	if (!n--) return 0;
	for (; *a && n && (*a == *b || fold(*a) == fold(*b)); a++, b++, n--);
	return fold(*a) - fold(*b);
}

int strcasecmp_l(const char *l, const char *r, locale_t loc)
{
	return strcasecmp(l, r);
}

int strncasecmp_l(const char *l, const char *r, size_t n, locale_t loc)
{
	return strncasecmp(l, r, n);
}
