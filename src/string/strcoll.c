/*
 * lib-spfxd — collation.  The library implements only the C/POSIX
 * collation order (byte order); UTF-8 locales collate identically, which
 * is code-point order because UTF-8 preserves it.
 */
#include <string.h>
#include <locale.h>

int strcoll(const char *l, const char *r)
{
	return strcmp(l, r);
}

int strcoll_l(const char *l, const char *r, locale_t loc)
{
	return strcmp(l, r);
}

size_t strxfrm(char *restrict d, const char *restrict s, size_t n)
{
	size_t len = strlen(s);
	if (len < n) memcpy(d, s, len + 1);
	return len;
}

size_t strxfrm_l(char *restrict d, const char *restrict s, size_t n, locale_t loc)
{
	return strxfrm(d, s, n);
}
