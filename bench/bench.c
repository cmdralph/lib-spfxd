/*
 * lib-spfxd benchmarks.
 *
 * One source, built twice: against lib-spfxd (lib/spfxd-gcc) and against
 * the host C library.  Each benchmark reports nanoseconds per operation as
 * the best of several repetitions, so one-off scheduling noise does not
 * dominate.  Output is one line per benchmark:
 *
 *     <name> <ns/op>
 *
 * bench/compare.sh joins the two outputs into a table.
 *
 *   bench [filter]      run only benchmarks whose name contains filter
 */
#define _GNU_SOURCE
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define REPS 5

static volatile uint64_t sink;          /* defeats dead-code elimination */
static const char *filter;

static double now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1e9 + ts.tv_nsec;
}

typedef void (*bench_fn)(long iters, void *arg);

/* Run fn(iters) REPS times and print the best ns/op. */
static void run(const char *name, bench_fn fn, long iters, void *arg)
{
	if (filter && !strstr(name, filter)) return;
	fn(iters / 10 + 1, arg);                /* warm caches and the allocator */
	double best = 1e300;
	for (int r = 0; r < REPS; r++) {
		double t0 = now_ns();
		fn(iters, arg);
		double t = (now_ns() - t0) / iters;
		if (t < best) best = t;
	}
	printf("%-28s %10.2f\n", name, best);
	fflush(stdout);
}

/* ------------------------------------------------------------------ */
/* string.h                                                           */
/* ------------------------------------------------------------------ */
static char *bufa, *bufb;
#define BUFSZ (1 << 20)

static void b_memcpy(long n, void *arg)
{
	size_t sz = (size_t)(uintptr_t)arg;
	for (long i = 0; i < n; i++) {
		memcpy(bufa + (i & 63), bufb, sz);
		__asm__ volatile("" ::: "memory");
	}
}

static void b_memmove(long n, void *arg)
{
	size_t sz = (size_t)(uintptr_t)arg;
	for (long i = 0; i < n; i++) {
		memmove(bufa + 1, bufa, sz);
		__asm__ volatile("" ::: "memory");
	}
}

static void b_memset(long n, void *arg)
{
	size_t sz = (size_t)(uintptr_t)arg;
	for (long i = 0; i < n; i++) {
		memset(bufa + (i & 63), (int)i, sz);
		__asm__ volatile("" ::: "memory");
	}
}

static void b_strlen(long n, void *arg)
{
	size_t sz = (size_t)(uintptr_t)arg;
	memset(bufa, 'x', sz);
	bufa[sz] = 0;
	uint64_t s = 0;
	for (long i = 0; i < n; i++) {
		char *volatile p = bufa;
		s += strlen(p);
	}
	sink = s;
}

static void b_strchr(long n, void *arg)
{
	size_t sz = (size_t)(uintptr_t)arg;
	memset(bufa, 'x', sz);
	bufa[sz - 1] = 'y';
	bufa[sz] = 0;
	uint64_t s = 0;
	for (long i = 0; i < n; i++) {
		char *volatile p = bufa;
		s += (uintptr_t)strchr(p, 'y');
	}
	sink = s;
}

static void b_memchr(long n, void *arg)
{
	size_t sz = (size_t)(uintptr_t)arg;
	memset(bufa, 'x', sz);
	bufa[sz - 1] = 'y';
	uint64_t s = 0;
	for (long i = 0; i < n; i++) {
		char *volatile p = bufa;
		s += (uintptr_t)memchr(p, 'y', sz);
	}
	sink = s;
}

static void b_strcmp(long n, void *arg)
{
	size_t sz = (size_t)(uintptr_t)arg;
	memset(bufa, 'x', sz);
	memset(bufb, 'x', sz);
	bufa[sz] = bufb[sz] = 0;
	bufb[sz - 1] = 'y';
	uint64_t s = 0;
	for (long i = 0; i < n; i++) {
		char *volatile p = bufa;
		s += strcmp(p, bufb) < 0;
	}
	sink = s;
}

static void b_memcmp(long n, void *arg)
{
	size_t sz = (size_t)(uintptr_t)arg;
	memset(bufa, 'x', sz);
	memset(bufb, 'x', sz);
	bufb[sz - 1] = 'y';
	uint64_t s = 0;
	for (long i = 0; i < n; i++) {
		char *volatile p = bufa;
		s += memcmp(p, bufb, sz) < 0;
	}
	sink = s;
}

static void b_strstr(long n, void *arg)
{
	(void)arg;
	size_t sz = 4096;
	for (size_t i = 0; i < sz; i++) bufa[i] = "abcdefgh"[i % 7];
	strcpy(bufa + sz - 16, "needle-in-stack");
	uint64_t s = 0;
	for (long i = 0; i < n; i++) {
		char *volatile p = bufa;
		s += (uintptr_t)strstr(p, "needle");
	}
	sink = s;
}

/* ------------------------------------------------------------------ */
/* malloc                                                             */
/* ------------------------------------------------------------------ */
static void b_malloc_free(long n, void *arg)
{
	size_t sz = (size_t)(uintptr_t)arg;
	for (long i = 0; i < n; i++) {
		void *volatile p = malloc(sz);
		free(p);
	}
}

/* Many live objects of mixed sizes, freed in a scrambled order. */
static void b_malloc_mixed(long n, void *arg)
{
	(void)arg;
	enum { LIVE = 4096 };
	static void *slot[LIVE];
	uint32_t x = 12345;
	for (long i = 0; i < n; i++) {
		x = x * 1103515245 + 12345;
		unsigned k = (x >> 8) % LIVE;
		free(slot[k]);
		slot[k] = malloc(16 + (x >> 20) % 1000);
	}
	for (int k = 0; k < LIVE; k++) { free(slot[k]); slot[k] = 0; }
}

static void b_realloc_grow(long n, void *arg)
{
	(void)arg;
	for (long i = 0; i < n; i++) {
		char *p = 0;
		for (size_t sz = 16; sz <= 4096; sz *= 2) {
			p = realloc(p, sz);
			p[sz - 1] = 1;
		}
		free(p);
	}
}

#define NTHREADS 4
struct tharg { long n; size_t sz; };

static void *malloc_thread(void *a)
{
	struct tharg *t = a;
	void *ring[64] = { 0 };
	for (long i = 0; i < t->n; i++) {
		free(ring[i & 63]);
		ring[i & 63] = malloc(t->sz);
	}
	for (int i = 0; i < 64; i++) free(ring[i]);
	return 0;
}

static void b_malloc_threads(long n, void *arg)
{
	pthread_t th[NTHREADS];
	struct tharg t = { n / NTHREADS, (size_t)(uintptr_t)arg };
	for (int i = 0; i < NTHREADS; i++) pthread_create(&th[i], 0, malloc_thread, &t);
	for (int i = 0; i < NTHREADS; i++) pthread_join(th[i], 0);
}

/* ------------------------------------------------------------------ */
/* stdio and conversions                                              */
/* ------------------------------------------------------------------ */
static void b_snprintf_int(long n, void *arg)
{
	(void)arg;
	char b[64];
	uint64_t s = 0;
	for (long i = 0; i < n; i++) s += snprintf(b, sizeof b, "%d %ld %x", (int)i, -i * 7919, (unsigned)i);
	sink = s;
}

static void b_snprintf_str(long n, void *arg)
{
	(void)arg;
	char b[128];
	uint64_t s = 0;
	for (long i = 0; i < n; i++) s += snprintf(b, sizeof b, "%s=%-10s|%5.3s", "key", "value", "truncate");
	sink = s;
}

static void b_snprintf_g(long n, void *arg)
{
	(void)arg;
	char b[64];
	uint64_t s = 0;
	double v = 1.0;
	for (long i = 0; i < n; i++) {
		s += snprintf(b, sizeof b, "%g", v);
		v = v * 1.0001 + 0.37;
	}
	sink = s;
}

static void b_snprintf_17g(long n, void *arg)
{
	(void)arg;
	char b[64];
	uint64_t s = 0;
	double v = 0.1;
	for (long i = 0; i < n; i++) {
		s += snprintf(b, sizeof b, "%.17g", v);
		v = v * 1.37 + 1e-3;
		if (v > 1e200) v = 0.1;
	}
	sink = s;
}

static char numbuf[1024][32];

static void prep_numbers(void)
{
	double v = 0.001;
	for (int i = 0; i < 1024; i++) {
		snprintf(numbuf[i], sizeof numbuf[i], "%.17g", v);
		v = v * 1.731 + 0.1;
		if (v > 1e100) v = 1e-5;
	}
}

static void b_strtod(long n, void *arg)
{
	(void)arg;
	double s = 0;
	for (long i = 0; i < n; i++) s += strtod(numbuf[i & 1023], 0);
	sink = (uint64_t)s;
}

static void b_strtol(long n, void *arg)
{
	(void)arg;
	static const char *nums[] = { "0", "42", "-123456789", "0x7fffffff", "9223372036854775807", "  +31415" };
	uint64_t s = 0;
	for (long i = 0; i < n; i++) s += (uint64_t)strtol(nums[i % 6], 0, 0);
	sink = s;
}

static void b_fwrite_lines(long n, void *arg)
{
	(void)arg;
	FILE *f = fopen("/dev/null", "w");
	if (!f) return;
	for (long i = 0; i < n; i++) fputs("a line of text that is written to a stream\n", f);
	fclose(f);
}

static void b_fprintf_file(long n, void *arg)
{
	(void)arg;
	FILE *f = fopen("/dev/null", "w");
	if (!f) return;
	for (long i = 0; i < n; i++) fprintf(f, "%ld: %s %d\n", i, "entry", (int)(i * 3));
	fclose(f);
}

static void b_sscanf(long n, void *arg)
{
	(void)arg;
	int a, b2;
	char w[32];
	uint64_t s = 0;
	for (long i = 0; i < n; i++) {
		s += sscanf("1234 word 5678", "%d %31s %d", &a, w, &b2);
		s += a + b2;
	}
	sink = s;
}

/* ------------------------------------------------------------------ */
/* qsort                                                              */
/* ------------------------------------------------------------------ */
static int cmp_int(const void *a, const void *b)
{
	int x = *(const int *)a, y = *(const int *)b;
	return (x > y) - (x < y);
}

static void b_qsort(long n, void *arg)
{
	size_t cnt = (size_t)(uintptr_t)arg;
	int *v = malloc(cnt * sizeof *v);
	for (long i = 0; i < n; i++) {
		uint32_t x = (uint32_t)i * 2654435761u;
		for (size_t k = 0; k < cnt; k++) { x = x * 1664525 + 1013904223; v[k] = (int)x; }
		qsort(v, cnt, sizeof *v, cmp_int);
	}
	sink = (uint64_t)v[0];
	free(v);
}

/* ------------------------------------------------------------------ */
/* libm                                                               */
/* ------------------------------------------------------------------ */
static double margs[4096];

static void prep_math(void)
{
	uint64_t x = 88172645463325252ull;
	for (int i = 0; i < 4096; i++) {
		x ^= x << 13; x ^= x >> 7; x ^= x << 17;
		margs[i] = (double)(x >> 11) * 0x1p-53;      /* [0, 1) */
	}
}

#define MATH1(fn, scale, off) \
	static void b_##fn(long n, void *arg) \
	{ \
		(void)arg; \
		double s = 0; \
		for (long i = 0; i < n; i++) s += fn(margs[i & 4095] * (scale) + (off)); \
		sink = (uint64_t)s; \
	}

MATH1(sin, 20.0, -10.0)
MATH1(cos, 20.0, -10.0)
MATH1(tan, 3.0, -1.5)
MATH1(exp, 100.0, -50.0)
MATH1(log, 1000.0, 0.001)
MATH1(log2, 1000.0, 0.001)
MATH1(atan, 20.0, -10.0)
MATH1(sqrt, 1000.0, 0.0)
MATH1(cbrt, 1000.0, -500.0)
MATH1(erf, 6.0, -3.0)
MATH1(tgamma, 10.0, 0.5)

static void b_pow(long n, void *arg)
{
	(void)arg;
	double s = 0;
	for (long i = 0; i < n; i++) s += pow(margs[i & 4095] * 10 + 0.1, margs[(i + 7) & 4095] * 20 - 10);
	sink = (uint64_t)s;
}

static void b_sinf(long n, void *arg)
{
	(void)arg;
	float s = 0;
	for (long i = 0; i < n; i++) s += sinf((float)margs[i & 4095] * 20.0f - 10.0f);
	sink = (uint64_t)s;
}

/* ------------------------------------------------------------------ */
/* threads                                                            */
/* ------------------------------------------------------------------ */
static void b_mutex(long n, void *arg)
{
	(void)arg;
	static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
	for (long i = 0; i < n; i++) {
		pthread_mutex_lock(&m);
		sink++;
		pthread_mutex_unlock(&m);
	}
}

static pthread_mutex_t cm = PTHREAD_MUTEX_INITIALIZER;
static void *mutex_thread(void *a)
{
	long n = *(long *)a;
	for (long i = 0; i < n; i++) {
		pthread_mutex_lock(&cm);
		sink++;
		pthread_mutex_unlock(&cm);
	}
	return 0;
}

static void b_mutex_contended(long n, void *arg)
{
	(void)arg;
	pthread_t th[NTHREADS];
	long per = n / NTHREADS;
	for (int i = 0; i < NTHREADS; i++) pthread_create(&th[i], 0, mutex_thread, &per);
	for (int i = 0; i < NTHREADS; i++) pthread_join(th[i], 0);
}

static void *noop_thread(void *a) { return a; }

static void b_thread_create(long n, void *arg)
{
	(void)arg;
	for (long i = 0; i < n; i++) {
		pthread_t t;
		pthread_create(&t, 0, noop_thread, 0);
		pthread_join(t, 0);
	}
}

static void b_getenv(long n, void *arg)
{
	(void)arg;
	uint64_t s = 0;
	for (long i = 0; i < n; i++) s += (uintptr_t)getenv("BENCH_NOT_SET");
	sink = s;
}

static void b_localtime(long n, void *arg)
{
	(void)arg;
	uint64_t s = 0;
	struct tm tm;
	for (long i = 0; i < n; i++) {
		time_t t = 1700000000 + i * 3607;
		localtime_r(&t, &tm);
		s += tm.tm_hour;
	}
	sink = s;
}

#define SZ(x) ((void *)(uintptr_t)(x))

int main(int argc, char **argv)
{
	filter = argc > 1 ? argv[1] : 0;
	bufa = aligned_alloc(4096, BUFSZ + 4096);
	bufb = aligned_alloc(4096, BUFSZ + 4096);
	memset(bufa, 1, BUFSZ + 4096);
	memset(bufb, 2, BUFSZ + 4096);
	prep_numbers();
	prep_math();
	setenv("TZ", "Europe/Berlin", 1);
	tzset();

	run("memcpy/16", b_memcpy, 20000000, SZ(16));
	run("memcpy/256", b_memcpy, 10000000, SZ(256));
	run("memcpy/4096", b_memcpy, 1000000, SZ(4096));
	run("memcpy/1M", b_memcpy, 2000, SZ(1 << 20));
	run("memmove/4096", b_memmove, 1000000, SZ(4096));
	run("memset/64", b_memset, 20000000, SZ(64));
	run("memset/4096", b_memset, 1000000, SZ(4096));
	run("strlen/16", b_strlen, 20000000, SZ(16));
	run("strlen/1024", b_strlen, 2000000, SZ(1024));
	run("strchr/1024", b_strchr, 2000000, SZ(1024));
	run("memchr/1024", b_memchr, 2000000, SZ(1024));
	run("strcmp/1024", b_strcmp, 2000000, SZ(1024));
	run("memcmp/1024", b_memcmp, 2000000, SZ(1024));
	run("strstr/4096", b_strstr, 200000, 0);

	run("malloc+free/32", b_malloc_free, 20000000, SZ(32));
	run("malloc+free/512", b_malloc_free, 20000000, SZ(512));
	run("malloc+free/64K", b_malloc_free, 2000000, SZ(65536));
	run("malloc/mixed", b_malloc_mixed, 5000000, 0);
	run("realloc/grow", b_realloc_grow, 500000, 0);
	run("malloc/4threads", b_malloc_threads, 8000000, SZ(64));

	run("snprintf/int", b_snprintf_int, 2000000, 0);
	run("snprintf/str", b_snprintf_str, 2000000, 0);
	run("snprintf/%g", b_snprintf_g, 1000000, 0);
	run("snprintf/%.17g", b_snprintf_17g, 1000000, 0);
	run("strtod", b_strtod, 2000000, 0);
	run("strtol", b_strtol, 10000000, 0);
	run("sscanf", b_sscanf, 1000000, 0);
	run("fputs/devnull", b_fwrite_lines, 5000000, 0);
	run("fprintf/devnull", b_fprintf_file, 2000000, 0);

	run("qsort/100", b_qsort, 20000, SZ(100));
	run("qsort/100000", b_qsort, 20, SZ(100000));

	run("sin", b_sin, 10000000, 0);
	run("cos", b_cos, 10000000, 0);
	run("tan", b_tan, 10000000, 0);
	run("exp", b_exp, 10000000, 0);
	run("log", b_log, 10000000, 0);
	run("log2", b_log2, 10000000, 0);
	run("pow", b_pow, 10000000, 0);
	run("atan", b_atan, 10000000, 0);
	run("sqrt", b_sqrt, 20000000, 0);
	run("cbrt", b_cbrt, 10000000, 0);
	run("erf", b_erf, 10000000, 0);
	run("tgamma", b_tgamma, 2000000, 0);
	run("sinf", b_sinf, 10000000, 0);

	run("mutex/uncontended", b_mutex, 20000000, 0);
	run("mutex/4threads", b_mutex_contended, 4000000, 0);
	run("pthread_create+join", b_thread_create, 20000, 0);
	run("getenv", b_getenv, 10000000, 0);
	run("localtime_r", b_localtime, 2000000, 0);
	return 0;
}
