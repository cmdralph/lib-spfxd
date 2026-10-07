/* lib-spfxd test — allocator: correctness, alignment, realloc, large
 * blocks, multithreaded stress, and hardening (detected misuse aborts). */
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "t.h"

static unsigned long long rs = 1;
static unsigned rnd(void) { rs = rs * 6364136223846793005ULL + 1442695040888963407ULL; return (unsigned)(rs >> 33); }

static int dies_with_abort(void (*fn)(void))
{
	pid_t p = fork();
	if (!p) {
		int nul = open("/dev/null", 1);
		dup2(nul, 2);
		fn();
		_exit(0);
	}
	int st;
	waitpid(p, &st, 0);
	return WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT;
}

/* volatile pointers keep the compiler from eliding the malloc/free pairs */
static void double_free(void) { char *volatile p = malloc(32); free(p); free(p); }
static void double_free_deep(void)
{
	char *volatile p = malloc(48), *volatile q = malloc(48);
	free(p);
	free(q);               /* p is no longer at the head of the bin */
	free(p);
}
static void bad_free(void) { char *volatile p = malloc(64); free(p + 16); }
static void big_double_free(void) { char *volatile p = malloc(1 << 20); free(p); free(p); }

#define NT 8
static void *stress(void *arg)
{
	unsigned long long s = (unsigned long long)(uintptr_t)arg;
	void *slots[256] = { 0 };
	size_t sizes[256] = { 0 };
	for (int it = 0; it < 100000; it++) {
		s = s * 6364136223846793005ULL + 1;
		unsigned r = (unsigned)(s >> 33), k = r & 255;
		if (slots[k]) {
			unsigned char *p = slots[k];
			for (size_t i = 0; i < sizes[k]; i += 61) if (p[i] != (unsigned char)k) return (void *)1;
			if (r & 0x100) {
				size_t ns = (r >> 9) % 5000;
				p = realloc(p, ns);
				if (ns && !p) return (void *)2;
				for (size_t i = sizes[k]; i < ns; i++) p[i] = (unsigned char)k;
				slots[k] = ns ? p : 0;
				sizes[k] = ns;
				if (!ns) free(p);
				continue;
			}
			free(p);
			slots[k] = 0;
		} else {
			size_t sz = (r >> 9) % (r & 0x200 ? 70000 : 300) + 1;
			unsigned char *p = malloc(sz);
			if (!p) return (void *)3;
			memset(p, k, sz);
			slots[k] = p;
			sizes[k] = sz;
		}
	}
	for (int k = 0; k < 256; k++) free(slots[k]);
	return 0;
}

/* blocks freed by a thread other than the allocating one */
static void *shared[4096];
static void *producer(void *a) { for (int i = 0; i < 4096; i++) shared[i] = malloc(16 + i % 500); return a; }
static void *consumer(void *a) { for (int i = 0; i < 4096; i++) free(shared[i]); return a; }

int main(void)
{
	/* basic properties */
	void *z = malloc(0);
	CHECK(z != NULL, "malloc(0) returns a unique pointer");
	free(z);
	free(NULL);
	for (size_t sz = 1; sz < 70000; sz = sz * 3 / 2 + 1) {
		unsigned char *p = malloc(sz);
		CHECK(p && ((uintptr_t)p & 15) == 0, "malloc(%zu) aligned", sz);
		CHECK(malloc_usable_size(p) >= sz, "usable size %zu", sz);
		memset(p, 0xab, sz);
		free(p);
	}
	unsigned *c = calloc(1000, sizeof *c);
	int zero = 1;
	for (int i = 0; i < 1000; i++) if (c[i]) zero = 0;
	CHECK(c && zero, "calloc zeroes");
	free(c);
	errno = 0;
	CHECK(!calloc(SIZE_MAX / 2, 4) && errno == ENOMEM, "calloc overflow");
	errno = 0;
	CHECK(!malloc(SIZE_MAX - 100) && errno == ENOMEM, "huge malloc fails");
	/* realloc keeps contents */
	char *r = malloc(10);
	strcpy(r, "contents");
	for (size_t sz = 16; sz < (1 << 22); sz *= 3) {
		r = realloc(r, sz);
		CHECK(r && !strcmp(r, "contents"), "realloc grow %zu", sz);
	}
	r = realloc(r, 9);
	CHECK(r && !strcmp(r, "contents"), "realloc shrink");
	free(r);
	CHECK(reallocarray(0, SIZE_MAX / 2, 3) == NULL && errno == ENOMEM, "reallocarray overflow");
	/* aligned allocation */
	for (size_t al = 8; al <= (1 << 23); al <<= 1) {
		void *p = 0;
		CHECK(posix_memalign(&p, al, 100) == 0 && ((uintptr_t)p & (al - 1)) == 0, "posix_memalign %zu", al);
		free(p);
		p = aligned_alloc(al, al * 2);
		CHECK(p && ((uintptr_t)p & (al - 1)) == 0, "aligned_alloc %zu", al);
		free(p);
	}
	void *p = 0;
	CHECK(posix_memalign(&p, 24, 8) == EINVAL, "posix_memalign bad alignment");
	p = memalign(4096, 1);
	CHECK(p && !((uintptr_t)p & 4095), "memalign");
	free(p);
	p = valloc(10);
	CHECK(p && !((uintptr_t)p & 4095), "valloc");
	free(p);
	/* large and huge blocks are returned to the system */
	for (int i = 0; i < 50; i++) {
		char *big = malloc(64 << 20);
		CHECK(big != NULL, "64 MiB allocation %d", i);
		if (!big) break;
		big[0] = big[(64 << 20) - 1] = 1;
		free(big);
	}
	/* single-thread random workload */
	CHECK(stress((void *)1) == 0, "random workload");
	/* multithreaded */
	pthread_t t[NT];
	for (long i = 0; i < NT; i++) pthread_create(&t[i], 0, stress, (void *)(i + 100));
	for (int i = 0; i < NT; i++) {
		void *res;
		pthread_join(t[i], &res);
		CHECK(res == 0, "thread workload %d: %p", i, res);
	}
	for (int round = 0; round < 10; round++) {
		pthread_t a, b;
		pthread_create(&a, 0, producer, 0);
		pthread_join(a, 0);
		pthread_create(&b, 0, consumer, 0);
		pthread_join(b, 0);
	}
	CHECK(1, "cross-thread frees");
	/* hardening */
	CHECK(dies_with_abort(double_free), "double free detected");
	CHECK(dies_with_abort(double_free_deep), "double free detected below the bin head");
	CHECK(dies_with_abort(bad_free), "invalid free detected");
	CHECK(dies_with_abort(big_double_free), "large double free detected");
	(void)rnd;
	return DONE();
}
