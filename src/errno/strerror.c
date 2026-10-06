/*
 * lib-spfxd — strerror / strerror_r / strerror_l.
 *
 * Unknown numbers produce "Unknown error N".  strerror() returns a
 * thread-local buffer only for unknown numbers, so it is thread safe in
 * practice for every real error code (which map to constant strings).
 *
 * strerror_r: the POSIX (XSI) form returning an int is exported as
 * __xpg_strerror_r; strerror_r itself follows the GNU convention of
 * returning a char * (the header maps the name per feature macros).
 */
#include <errno.h>
#include <string.h>
#include <locale.h>
#include "pthread_impl.h"


static char *unknown(int e, char *buf, size_t n)
{
	char tmp[32], *p = tmp + sizeof tmp;
	unsigned u = e < 0 ? 0U - (unsigned)e : (unsigned)e;
	*--p = 0;
	do *--p = (char)('0' + u % 10); while (u /= 10);
	if (e < 0) *--p = '-';
	static const char pre[] = "Unknown error ";
	size_t pl = sizeof pre - 1, nl = strlen(p);
	if (n) {
		size_t k = pl < n - 1 ? pl : n - 1;
		memcpy(buf, pre, k);
		size_t r = nl < n - 1 - k ? nl : n - 1 - k;
		memcpy(buf + k, p, r);
		buf[k + r] = 0;
	}
	return buf;
}

char *strerror(int e)
{
	const char *s = __strerror_lookup(e);
	if (s) return (char *)s;
	char *buf = __self()->strerror_buf;
	return unknown(e, buf, sizeof __self()->strerror_buf);
}

char *strerror_l(int e, locale_t loc)
{
	return strerror(e);
}

int __xpg_strerror_r(int e, char *buf, size_t n)
{
	const char *s = __strerror_lookup(e);
	if (!s) {
		unknown(e, buf, n);
		return EINVAL;
	}
	size_t l = strlen(s);
	if (l >= n) {
		if (n) {
			memcpy(buf, s, n - 1);
			buf[n - 1] = 0;
		}
		return ERANGE;
	}
	memcpy(buf, s, l + 1);
	return 0;
}

char *__gnu_strerror_r(int e, char *buf, size_t n)
{
	const char *s = __strerror_lookup(e);
	if (s) return (char *)s;
	return unknown(e, buf, n);
}
weak_alias(__gnu_strerror_r, strerror_r);
