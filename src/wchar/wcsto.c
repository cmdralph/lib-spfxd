/*
 * lib-spfxd — wcstol family and wcstod family.  The longest prefix made
 * only of characters that can appear in a number is narrowed to ASCII and
 * parsed by the byte-string converters, then the end pointer is mapped
 * back.  Any non-ASCII wide character ends the number.
 */
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include <inttypes.h>
#include <string.h>
#include "stdio_impl.h"


/* Copy a candidate numeral (with leading white space) into buf. */
static size_t narrow(const wchar_t *s, char *buf, size_t cap)
{
	size_t i = 0, k = 0;
	while (iswspace((wint_t)s[i])) i++;
	/* represent leading space by a single ' ' to keep offsets computable */
	size_t ws = i;
	if (ws) buf[k++] = ' ';
	for (; k + 1 < cap && s[i] > 0 && s[i] < 0x80; i++) buf[k++] = (char)s[i];
	buf[k] = 0;
	return ws;
}

static wchar_t *map_end(const wchar_t *s, const char *buf, const char *end, size_t ws)
{
	if (end == buf) return (wchar_t *)s;
	size_t off = (size_t)(end - buf);
	return (wchar_t *)s + off + (ws ? ws - 1 : 0);
}

#define NUMBUF 512

long long wcstoll(const wchar_t *restrict s, wchar_t **restrict end, int base)
{
	char buf[NUMBUF], *e;
	size_t ws = narrow(s, buf, sizeof buf);
	long long r = strtoll(buf, &e, base);
	if (end) *end = map_end(s, buf, e, ws);
	return r;
}

unsigned long long wcstoull(const wchar_t *restrict s, wchar_t **restrict end, int base)
{
	char buf[NUMBUF], *e;
	size_t ws = narrow(s, buf, sizeof buf);
	unsigned long long r = strtoull(buf, &e, base);
	if (end) *end = map_end(s, buf, e, ws);
	return r;
}

long wcstol(const wchar_t *restrict s, wchar_t **restrict end, int base)
{
	return (long)wcstoll(s, end, base);
}

unsigned long wcstoul(const wchar_t *restrict s, wchar_t **restrict end, int base)
{
	return (unsigned long)wcstoull(s, end, base);
}

intmax_t wcstoimax(const wchar_t *restrict s, wchar_t **restrict end, int base)
{
	return wcstoll(s, end, base);
}

uintmax_t wcstoumax(const wchar_t *restrict s, wchar_t **restrict end, int base)
{
	return wcstoull(s, end, base);
}

static long double wcsto_fp(const wchar_t *restrict s, wchar_t **restrict end, int prec)
{
	char small[NUMBUF], *buf = small, *e;
	size_t len = 0;
	/* floating-point numerals can be arbitrarily long: size the buffer */
	while (s[len] > 0 && s[len] < 0x80) len++;
	if (len + 2 > sizeof small) buf = malloc(len + 2);
	if (!buf) buf = small, len = sizeof small - 2;
	size_t ws = narrow(s, buf, len + 2);
	long double r = __strtold_internal(buf, &e, prec);
	if (end) *end = map_end(s, buf, e, ws);
	if (buf != small) free(buf);
	return r;
}

float wcstof(const wchar_t *restrict s, wchar_t **restrict end) { return (float)wcsto_fp(s, end, 0); }
double wcstod(const wchar_t *restrict s, wchar_t **restrict end) { return (double)wcsto_fp(s, end, 1); }
long double wcstold(const wchar_t *restrict s, wchar_t **restrict end) { return wcsto_fp(s, end, 2); }
