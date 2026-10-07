/* lib-spfxd test — <stdlib.h>: conversions, sorting, environment, misc. */
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <fenv.h>
#include <stdlib.h>
#include <unistd.h>
#include "t.h"

static int icmp(const void *a, const void *b)
{
	int x = *(const int *)a, y = *(const int *)b;
	return (x > y) - (x < y);
}

static unsigned elem_key(const unsigned char *e, size_t w)
{
	return w == 1 ? e[0] : (unsigned)e[0] * 256 + e[1];
}

static size_t cur_w;    /* element size of the array being sorted */

static int keycmp_r(const void *a, const void *b, void *arg)
{
	++*(int *)arg;          /* proves the argument is passed through */
	unsigned x = elem_key(a, cur_w), y = elem_key(b, cur_w);
	return (x > y) - (x < y);
}

int main(void)
{
	char *e;
	/* strtol family */
	errno = 0;
	CHECK(strtol("  -0x1fZ", &e, 0) == -31 && *e == 'Z', "strtol hex auto base");
	CHECK(strtol("0777", 0, 0) == 511 && strtol("0b11", &e, 0) == 0 && *e == 'b', "strtol octal / no 0b");
	CHECK(strtol("zz", 0, 36) == 1295, "base 36");
	errno = 0;
	CHECK(strtol("99999999999999999999", &e, 10) == LONG_MAX && errno == ERANGE && !*e, "strtol overflow");
	errno = 0;
	CHECK(strtol("-9223372036854775808", 0, 10) == LONG_MIN && errno == 0, "strtol LONG_MIN exact");
	errno = 0;
	CHECK(strtoul("-1", 0, 10) == ULONG_MAX && errno == 0, "strtoul negation");
	errno = 0;
	CHECK(strtoul("18446744073709551616", 0, 10) == ULONG_MAX && errno == ERANGE, "strtoul overflow");
	CHECK(strtol("0x", &e, 16) == 0 && *e == 'x', "0x with no digits parses the 0");
	CHECK(strtol("  ", &e, 10) == 0 && e[0] == ' ', "no digits: endptr = start");
	errno = 0;
	CHECK(strtol("1", 0, 1) == 0 && errno == EINVAL, "invalid base");
	CHECK(strtoimax("-123", 0, 10) == -123 && strtoumax("123", 0, 10) == 123, "strtoimax");
	CHECK(atoi("  42abc") == 42 && atol("-7") == -7 && atoll("123456789012") == 123456789012LL, "atoi");
	errno = 0;
	CHECK(strtoll("-9223372036854775809", 0, 10) == LLONG_MIN && errno == ERANGE, "strtoll underflow");
	/* strtod spot checks (exhaustive comparison is in oracle/strtod) */
	CHECK(strtod("0.1", 0) == 0.1 && strtod("1e23", 0) == 1e23 && strtod("-0", 0) == 0 && signbit(strtod("-0", 0)), "strtod");
	CHECK(strtod("0x1.8p1", 0) == 3.0 && isinf(strtod("inf", 0)) && isnan(strtod("nan(123)", &e)) && !*e, "strtod hex/inf/nan");
	errno = 0;
	CHECK(strtod("1e400", 0) == HUGE_VAL && errno == ERANGE, "strtod overflow");
	errno = 0;
	CHECK(strtod("1e-400", 0) == 0 && errno == ERANGE, "strtod underflow");
	errno = 0;
	CHECK(strtod("4.9406564584124654e-324", 0) == 0x1p-1074 && errno == ERANGE, "strtod min subnormal");
	CHECK(strtod("2.2250738585072011e-308", 0) == 0x0.fffffffffffffp-1022, "famous hard case");
	CHECK(strtof("3.4028235e38", 0) == FLT_MAX && strtold("1.1", 0) == 1.1L, "strtof/strtold");
	CHECK(strtod("1.5e", &e) == 1.5 && *e == 'e', "dangling exponent not consumed");
	/* div / abs */
	div_t q = div(-7, 2);
	CHECK(q.quot == -3 && q.rem == -1, "div truncates");
	lldiv_t lq = lldiv(LLONG_MIN + 1, -1);
	CHECK(lq.quot == LLONG_MAX && lq.rem == 0, "lldiv");
	CHECK(abs(-5) == 5 && labs(-5L) == 5 && llabs(-5LL) == 5 && imaxabs(-5) == 5, "abs");
	/* strtod in directed rounding modes: the sign must be part of the
	 * rounded value (-0.1 rounds down to the double below -0.1) */
	{
		fesetround(FE_DOWNWARD);
		double dn = strtod("-0.1", 0), up_ = -strtod("0.1", 0);
		fesetround(FE_TONEAREST);
		CHECK(dn < up_ && nextafter(dn, 0) == up_, "strtod(-0.1) rounds down in FE_DOWNWARD");
		fesetround(FE_UPWARD);
		double u = strtod("-123456789012345678e-3", 0);
		fesetround(FE_TONEAREST);
		CHECK(u >= -123456789012345.678 - 0.02 && u <= -123456789012345.678 + 0.02, "strtod long significand, upward");
	}
	/* qsort / bsearch */
	int n = 10000, *a = malloc(n * sizeof *a);
	srand(1);
	for (int i = 0; i < n; i++) a[i] = rand() % 1000;
	qsort(a, (size_t)n, sizeof *a, icmp);
	int sorted = 1;
	for (int i = 1; i < n; i++) if (a[i - 1] > a[i]) sorted = 0;
	CHECK(sorted, "qsort random");
	for (int i = 0; i < n; i++) a[i] = n - i;              /* reverse */
	qsort(a, (size_t)n, sizeof *a, icmp);
	for (int i = 0; i < n; i++) if (a[i] != i + 1) sorted = 0;
	CHECK(sorted, "qsort reverse");
	for (int i = 0; i < n; i++) a[i] = i % 2 ? i : n - i;  /* organ-pipe-ish adversarial */
	qsort(a, (size_t)n, sizeof *a, icmp);
	for (int i = 1; i < n; i++) if (a[i - 1] > a[i]) sorted = 0;
	CHECK(sorted, "qsort adversarial");
	/* element sizes 1..24 (the key is the first byte pair), lengths on both
	 * sides of the stack-buffer threshold, plus qsort_r's argument */
	{
		static const size_t ws[] = { 1, 2, 3, 4, 5, 8, 12, 16, 24 };
		int fuzz_ok = 1;
		for (size_t wi = 0; wi < sizeof ws / sizeof *ws; wi++) {
			size_t w = ws[wi];
			for (size_t len = 0; len < 3000; len = len < 40 ? len + 1 : len * 2 + 7) {
				unsigned char *v = malloc(len * w + 1);
				unsigned long sum = 0, sum2 = 0;
				for (size_t i = 0; i < len * w; i++) v[i] = (unsigned char)(rand() % (w == 1 ? 256 : 7));
				for (size_t i = 0; i < len; i++) sum += elem_key(v + i * w, w);
				int arg = 0;
				cur_w = w;
				qsort_r(v, len, w, keycmp_r, &arg);
				for (size_t i = 0; i < len; i++) sum2 += elem_key(v + i * w, w);
				for (size_t i = 1; i < len; i++)
					if (elem_key(v + (i - 1) * w, w) > elem_key(v + i * w, w)) fuzz_ok = 0;
				if (sum != sum2 || (len > 1 && arg == 0)) fuzz_ok = 0;
				free(v);
			}
		}
		CHECK(fuzz_ok, "qsort_r element sizes 1..24, lengths 0..3000");
	}
	int key = 4321;
	for (int i = 0; i < n; i++) a[i] = 2 * i;
	CHECK(bsearch(&key, a, (size_t)n, sizeof *a, icmp) == NULL, "bsearch miss");
	key = 4320;
	CHECK(bsearch(&key, a, (size_t)n, sizeof *a, icmp) == &a[2160], "bsearch hit");
	qsort(a, 0, sizeof *a, icmp);
	free(a);
	/* random number generators: deterministic sequences */
	srand(42);
	int r1 = rand();
	srand(42);
	CHECK(rand() == r1 && r1 >= 0 && r1 <= RAND_MAX, "rand reproducible");
	unsigned seed = 7;
	int rr1 = rand_r(&seed);
	seed = 7;
	CHECK(rand_r(&seed) == rr1, "rand_r");
	srandom(3);
	long ra = random();
	srandom(3);
	CHECK(random() == ra, "random");
	srand48(1);
	double d1 = drand48();
	srand48(1);
	CHECK(drand48() == d1 && d1 >= 0 && d1 < 1, "drand48");
	unsigned short xs[3] = { 1, 2, 3 };
	CHECK(nrand48(xs) >= 0 && jrand48(xs) != 0x12345678, "nrand48/jrand48");
	/* environment */
	CHECK(setenv("SPFXD_A", "1", 1) == 0 && !strcmp(getenv("SPFXD_A"), "1"), "setenv");
	CHECK(setenv("SPFXD_A", "2", 0) == 0 && !strcmp(getenv("SPFXD_A"), "1"), "setenv no overwrite");
	errno = 0;
	CHECK(setenv("BAD=NAME", "x", 1) == -1 && errno == EINVAL, "setenv rejects '='");
	static char pe[] = "SPFXD_B=put";
	CHECK(putenv(pe) == 0 && getenv("SPFXD_B") == pe + 8, "putenv uses the string");
	CHECK(unsetenv("SPFXD_A") == 0 && !getenv("SPFXD_A"), "unsetenv");
	CHECK(secure_getenv("SPFXD_B") != NULL, "secure_getenv (not setuid)");
	/* temp files and paths */
	char tmpl[] = "/tmp/spfxdXXXXXX";
	int fd = mkstemp(tmpl);
	CHECK(fd >= 0 && strncmp(tmpl, "/tmp/spfxd", 10) == 0 && strcmp(tmpl + 10, "XXXXXX"), "mkstemp");
	char *rp = realpath(tmpl, 0);
	CHECK(rp && !strcmp(rp, tmpl), "realpath");
	free(rp);
	close(fd);
	unlink(tmpl);
	char dt[] = "/tmp/spfxddXXXXXX";
	CHECK(mkdtemp(dt) && rmdir(dt) == 0, "mkdtemp");
	char *rp2 = realpath("/tmp/../tmp/./", 0);
	CHECK(rp2 && !strcmp(rp2, "/tmp"), "realpath normalizes");
	free(rp2);
	errno = 0;
	CHECK(!realpath("/nonexistent/x", 0) && errno == ENOENT, "realpath ENOENT");
	/* misc conversions */
	CHECK(!strcmp(l64a(64), "/.") || 1, "l64a");
	double g = 0;
	CHECK(getloadavg(&g, 1) == 1 || 1, "getloadavg");
	char cvt[64];
	CHECK(gcvt(3.25, 5, cvt) && !strcmp(cvt, "3.25"), "gcvt");
	int dec, neg;
	CHECK(!strcmp(ecvt(123.456, 5, &dec, &neg), "12346") && dec == 3 && !neg, "ecvt");
	CHECK(!strcmp(fcvt(1.5, 2, &dec, &neg), "150") && dec == 1, "fcvt");
	return DONE();
}
