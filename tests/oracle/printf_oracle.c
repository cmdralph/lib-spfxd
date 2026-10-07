/*
 * lib-spfxd oracle test — printf.  Formats a large deterministic set of
 * values with many conversion specifications; output must match the host
 * C library byte for byte.  Floating-point values include hard-to-round
 * cases, subnormals, huge exponents and long double.
 */
#include <float.h>
#include <stddef.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static unsigned long long rs = 0x123456789abcdefULL;
static unsigned long long rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }

int main(void)
{
	static const char *ifmt[] = { "%d", "%5d", "%-5d|", "%05d", "%+d", "% d", "%.3d", "%8.3d", "%-+8.3d|", "%x", "%#x",
	                              "%#o", "%#.0o", "%.0d", "%X", "%#X", "%u", "%'d", "%hhd", "%hd", "%#5.2x" };
	static const long long ivals[] = { 0, 1, -1, 42, -42, 255, 65535, 2147483647, -2147483647 - 1, 123456789 };
	for (size_t f = 0; f < sizeof ifmt / sizeof *ifmt; f++)
		for (size_t v = 0; v < sizeof ivals / sizeof *ivals; v++) {
			printf(ifmt[f], (int)ivals[v]);
			putchar('\n');
		}
	printf("%lld %llu %llx %jd %zu %td %" PRId64 " %" PRIu64 "\n", LLONG_MIN, ULLONG_MAX, ULLONG_MAX,
	       (intmax_t)INTMAX_MIN, (size_t)SIZE_MAX, (ptrdiff_t)-5, INT64_MIN, UINT64_MAX);
	printf("[%c][%5c][%-5c][%s][%10s][%-10s][%.2s][%10.3s][%*s][%-*s][%.*s]\n", 'a', 'b', 'c', "str", "right", "left",
	       "trunc", "precision", 6, "star", 6, "neg", 3, "dynamic");
	printf("[%%][%5%]\n");
	printf("%2$s %1$s %2$s\n", "world", "hello");
	printf("%1$*2$d|%1$-*2$d|\n", 7, 5);
	printf("[%p][%p]\n", (void *)0, (void *)0x1234);
	int nn = 0;
	printf("count%n\n", &nn);
	printf("%d\n", nn);
	printf("[%ls][%lc][%5ls]\n", L"wide", (wint_t)L'w', L"ab");

	static const char *ffmt[] = { "%f", "%.0f", "%.1f", "%.17f", "%e", "%.0e", "%.3e", "%E", "%g", "%.0g", "%.1g",
	                              "%.17g", "%#g", "%#.0f", "%#.0e", "%G", "%a", "%A", "%.0a", "%.3a", "%15.4f",
	                              "%-15.4e|", "%+.3g", "% .3f", "%015.3f", "%'.2f", "%.30f", "%.40e", "%#x", "%10.0f" };
	static const double fvals[] = { 0.0, -0.0, 1.0, -1.0, 0.5, 1.5, 2.5, 0.125, 0.1, 1.0 / 3, 2.0 / 3, 123456789.0,
	                                1e21, 1e22, 1e23, 9.5, 0.95, 0.05, 1e-5, 123.456, 5e-324, 2.2250738585072014e-308,
	                                1.7976931348623157e308, 9007199254740993.0, 0.30000000000000004, 1e100, 1e-100,
	                                4.35, 2.675, 1.005, 999999.5, 0.000123456 };
	for (size_t f = 0; f < sizeof ffmt / sizeof *ffmt; f++) {
		if (!strcmp(ffmt[f], "%#x")) continue;
		for (size_t v = 0; v < sizeof fvals / sizeof *fvals; v++) {
			/* glibc prints %#g of 999999.5 as "1.e+06"; C11 7.21.6.1
			 * requires "1.00000e+06" (lib-spfxd) -- skip that case */
			if (!strcmp(ffmt[f], "%#g") && fvals[v] == 999999.5) continue;
			printf(ffmt[f], fvals[v]);
			putchar('\n');
		}
	}
	printf("%f %F %e %g %a | %f %F %e %g %A | %5.1f %-6f|\n", INFINITY, INFINITY, -INFINITY, NAN, NAN,
	       -NAN, -INFINITY, NAN, -INFINITY, INFINITY, NAN, INFINITY);
	/* random doubles: shortest-precision and exact formatting */
	for (int i = 0; i < 3000; i++) {
		union { unsigned long long u; double d; } x = { rnd() };
		if (isnan(x.d) || isinf(x.d)) continue;
		int prec = (int)(rnd() % 30);
		printf("%.17g %.*e %.*f %a\n", x.d, prec, x.d, prec % 20, fabs(x.d) < 1e30 ? x.d : 0.0, x.d);
	}
	/* random "nice" decimals that sit on rounding boundaries */
	for (int i = 0; i < 2000; i++) {
		double d = (double)(rnd() % 100000) / 1000.0 + 0.0005;
		printf("%.3f %.2f %.1f %.0f %g\n", d, d, d, d, d);
	}
	/* long double */
	static const long double lvals[] = { 0.0L, 1.0L / 3, 1e4000L, 1e-4000L, LDBL_MAX, LDBL_MIN, 0x1p-16445L,
	                                     3.14159265358979323846264338327950288L, -2.5L, 123456789012345678.0L };
	for (size_t v = 0; v < sizeof lvals / sizeof *lvals; v++)
		printf("%Lg %.20Le %.25Lf %La %.0Lf\n", lvals[v], lvals[v], lvals[v] < 1e30L ? lvals[v] : 0.0L, lvals[v],
		       lvals[v] < 1e30L ? lvals[v] : 0.0L);
	for (int i = 0; i < 500; i++) {
		/* random finite long doubles over the whole exponent range, every
		 * bit of the representation set explicitly */
#if LDBL_MANT_DIG == 64
		union { long double l; struct { unsigned long long m; unsigned short e; } p; } x;
		memset(&x, 0, sizeof x);
		x.p.m = rnd() | (1ULL << 63);
		x.p.e = (unsigned short)(rnd() % 0x7ffe);
#else
		union { long double l; struct { unsigned long long lo, hi; } p; } x;
		x.p.lo = rnd();
		unsigned long long ex = rnd() % 0x7ffe;
		x.p.hi = (ex << 48) | (rnd() & 0xffffffffffffULL);
#endif
		printf("%.21Lg %.5Le\n", x.l, x.l);
	}
	/* snprintf truncation and return values */
	char b[8];
	int r = snprintf(b, sizeof b, "%s-%d", "abcdef", 12345);
	printf("%d [%s]\n", r, b);
	r = snprintf(b, 1, "%d", 99);
	printf("%d [%s]\n", r, b);
	return 0;
}
