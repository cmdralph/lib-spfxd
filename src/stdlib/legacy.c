/* lib-spfxd — legacy conversions: ecvt, fcvt, gcvt, l64a, a64l. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ecvt/fcvt return digits only, with the decimal point position in *dp
 * and the sign in *sign; computed through the exact printf engine. */
char *ecvt(double x, int n, int *dp, int *sign)
{
	static char buf[32];
	char tmp[48];
	if (n < 1) n = 1;
	if (n > 17) n = 17;
	snprintf(tmp, sizeof tmp, "%.*e", n - 1, x);
	*sign = tmp[0] == '-';
	const char *p = tmp + *sign;
	int j = 0;
	for (; *p && *p != 'e'; p++)
		if (*p != '.') buf[j++] = *p;
	buf[j] = 0;
	*dp = x == 0 ? 1 : atoi(p + 1) + 1;
	return buf;
}

char *fcvt(double x, int n, int *dp, int *sign)
{
	static char buf[1100];
	char tmp[1120];
	if (n < 0) n = 0;
	if (n > 700) n = 700;
	snprintf(tmp, sizeof tmp, "%.*f", n, x);
	*sign = tmp[0] == '-';
	const char *p = tmp + *sign;
	int j = 0, ip = 0, seen = 0;
	/* skip leading zeros of the integer part, counting integer digits */
	for (; *p == '0' && p[1] != '.' && p[1]; p++);
	for (; *p; p++) {
		if (*p == '.') { seen = 1; continue; }
		if (!seen) ip++;
		buf[j++] = *p;
	}
	buf[j] = 0;
	*dp = ip;
	if (buf[0] == '0' && ip == 1) {
		/* value below one: strip the leading zero and count zeros */
		int k = 1, z = 0;
		while (buf[k] == '0') k++, z++;
		memmove(buf, buf + k, (size_t)(j - k + 1));
		*dp = buf[0] ? -z : 0;
	}
	return buf;
}

char *gcvt(double x, int n, char *b)
{
	sprintf(b, "%.*g", n, x);
	return b;
}

static const char b64[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

char *l64a(long x0)
{
	static char s[7];
	uint32_t x = (uint32_t)x0;
	char *p = s;
	for (; x && p < s + 6; x >>= 6) *p++ = b64[x & 63];
	*p = 0;
	return s;
}

long a64l(const char *s)
{
	uint32_t x = 0;
	for (int e = 0; e < 36 && *s; e += 6, s++) {
		const char *d = strchr(b64, *s);
		if (!d || !*s) break;
		x |= (uint32_t)(d - b64) << e;
	}
	return (int32_t)x;
}
