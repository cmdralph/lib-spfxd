/*
 * lib-spfxd — exhaustive check of the float fast paths: for every float
 * x, f(x) must equal the double function's result rounded once to float
 * (the reference float.c and the fast paths both promise exactly that).
 *
 *   float_exhaustive <function> [threads]
 */
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *fn;
static int nthreads = 4;
static unsigned long long bad[64];

static float ref(float x)
{
	double d = x;
	if (!strcmp(fn, "sinf")) d = sin(d);
	else if (!strcmp(fn, "cosf")) d = cos(d);
	else if (!strcmp(fn, "tanf")) d = tan(d);
	else if (!strcmp(fn, "expf")) d = exp(d);
	else if (!strcmp(fn, "exp2f")) d = exp2(d);
	else if (!strcmp(fn, "logf")) d = log(d);
	else if (!strcmp(fn, "log2f")) d = log2(d);
	else if (!strcmp(fn, "log10f")) d = log10(d);
	return (float)d;
}

static float fast(float x)
{
	if (!strcmp(fn, "sinf")) return sinf(x);
	if (!strcmp(fn, "cosf")) return cosf(x);
	if (!strcmp(fn, "tanf")) return tanf(x);
	if (!strcmp(fn, "expf")) return expf(x);
	if (!strcmp(fn, "exp2f")) return exp2f(x);
	if (!strcmp(fn, "logf")) return logf(x);
	if (!strcmp(fn, "log2f")) return log2f(x);
	return log10f(x);
}

static void *work(void *arg)
{
	long t = (long)arg;
	for (uint64_t i = (uint64_t)t; i < (1ull << 32); i += (uint64_t)nthreads) {
		uint32_t b = (uint32_t)i;
		float x, a, r;
		memcpy(&x, &b, 4);
		a = fast(x);
		r = ref(x);
		uint32_t ab, rb;
		memcpy(&ab, &a, 4);
		memcpy(&rb, &r, 4);
		if (ab != rb && !(isnan(a) && isnan(r))) {
			if (bad[t]++ < 5) printf("%s(%a) = %a, expected %a\n", fn, x, a, r);
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	fn = argv[1];
	if (argc > 2) nthreads = atoi(argv[2]);
	pthread_t th[64];
	for (long t = 0; t < nthreads; t++) pthread_create(&th[t], 0, work, (void *)t);
	unsigned long long total = 0;
	for (long t = 0; t < nthreads; t++) { pthread_join(th[t], 0); total += bad[t]; }
	printf("%s: %llu mismatches over all floats\n", fn, total);
	return total != 0;
}
