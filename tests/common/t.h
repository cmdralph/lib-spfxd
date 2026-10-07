/*
 * lib-spfxd test helpers.  Each test program is self-checking: CHECK
 * prints a FAIL line (with file and line) for every failed condition and
 * DONE() prints a summary and yields the exit status.
 */
#ifndef T_H
#define T_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int t_fails, t_checks;

#define CHECK(cond, ...) do { \
	t_checks++; \
	if (!(cond)) { \
		t_fails++; \
		if (t_fails <= 50) { \
			printf("FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); \
			printf(__VA_ARGS__); \
			printf("\n"); \
			fflush(stdout); \
		} \
	} \
} while (0)

#define CHECK_EQ_INT(a, b) do { long long _a = (long long)(a), _b = (long long)(b); \
	CHECK(_a == _b, "%s = %lld, expected %lld", #a, _a, _b); } while (0)
#define CHECK_EQ_STR(a, b) do { const char *_a = (a), *_b = (b); \
	CHECK(_a && _b && !strcmp(_a, _b), "%s = \"%s\", expected \"%s\"", #a, _a ? _a : "(null)", _b ? _b : "(null)"); } while (0)

#define SKIP(msg) printf("SKIP %s:%d: %s\n", __FILE__, __LINE__, msg)

#define DONE() (printf("%s: %d checks, %d failures\n", __FILE__, t_checks, t_fails), t_fails != 0)

#endif
