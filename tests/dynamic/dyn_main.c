/*
 * lib-spfxd test — dynamic linking: DT_NEEDED order and constructors,
 * copy relocations (data and environ), general-dynamic TLS in a library,
 * dlopen/dlsym/dlerror/dladdr/dl_iterate_phdr, RTLD_NEXT, dynamic TLS of a
 * dlopen'ed module in several threads.
 */
#include <dlfcn.h>
#include <link.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int dep_counter, dep_order[8], dep_norder, mid_ctor_saw;
extern int dep_get_tls(void), *dep_tls_addr(void), dep_value(void), mid_value(void);
extern char **environ;

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int prog_exported(int x) { return x + 1; }

/* interpose strlen and forward with RTLD_NEXT */
static int strlen_calls;
size_t strlen(const char *s)
{
	static size_t (*real)(const char *);
	if (!real) real = (size_t (*)(const char *))dlsym(RTLD_NEXT, "strlen");
	strlen_calls++;
	return real(s);
}

static int phdr_cb(struct dl_phdr_info *i, size_t sz, void *d)
{
	int *n = d;
	(*n)++;
	(void)sz;
	return 0;
}

static long (*tls_get)(void);
static void (*tls_set)(long);
static void *thr(void *arg)
{
	long v = (long)arg;
	long first = tls_get();
	tls_set(v);
	for (int i = 0; i < 1000; i++) if (tls_get() != v) return (void *)-1;
	return (void *)(first == 1234 ? 0 : -2L);
}

int main(void)
{
	CHECK(dep_norder == 2 && dep_order[0] == 1 && dep_order[1] == 2 && mid_ctor_saw == 1,
	      "constructor order %d %d %d", dep_norder, dep_order[0], dep_order[1]);
	/* copy relocation: library and program must see the same object */
	CHECK(dep_counter == 100 && dep_value() == 100, "copy reloc initial");
	dep_counter = 5;
	CHECK(dep_value() == 5 && mid_value() == 6, "copy reloc shared %d", dep_value());
	CHECK(dep_get_tls() == 62, "library TLS %d", dep_get_tls());
	*dep_tls_addr() = 8;
	CHECK(dep_get_tls() == 63, "library TLS write");
	/* environ copy relocation vs libc's own view */
	setenv("SPFXD_TEST_VAR", "yes", 1);
	int found = 0;
	for (char **e = environ; *e; e++) if (!strcmp(*e, "SPFXD_TEST_VAR=yes")) found = 1;
	CHECK(found && !strcmp(getenv("SPFXD_TEST_VAR"), "yes"), "environ shared with libc");

	int n = 0;
	dl_iterate_phdr(phdr_cb, &n);
	CHECK(n >= 4, "dl_iterate_phdr count %d", n);

	Dl_info di;
	CHECK(dladdr((void *)dep_value, &di) && di.dli_sname && !strcmp(di.dli_sname, "dep_value") &&
	      strstr(di.dli_fname, "libdep.so"), "dladdr");

	/* dlopen errors */
	CHECK(!dlopen("libdoesnotexist.so", RTLD_NOW) && dlerror(), "dlopen missing");
	CHECK(!dlerror(), "dlerror cleared");

	void *h = dlopen("libplug.so", RTLD_NOW | RTLD_LOCAL);
	CHECK(h, "dlopen plugin: %s", dlerror());
	if (h) {
		int *ctor = dlsym(h, "plug_ctor");
		CHECK(ctor && *ctor == 1, "plugin constructor");
		int (*call)(int) = (int (*)(int))dlsym(h, "plug_call_prog");
		CHECK(call && call(4) == 10, "plugin calls into program");
		CHECK(!dlsym(RTLD_DEFAULT, "plug_tls_get"), "RTLD_LOCAL not global");
		tls_get = (long (*)(void))dlsym(h, "plug_tls_get");
		tls_set = (void (*)(long))dlsym(h, "plug_tls_set");
		CHECK(tls_get && tls_get() == 1234, "dynamic TLS initial");
		long *tp = dlsym(h, "plug_tls");
		CHECK(tp && *tp == 1234, "dlsym of TLS symbol");
		tls_set(99);
		CHECK(tls_get() == 99 && *tp == 99, "dynamic TLS write");
		pthread_t t[4];
		for (long i = 0; i < 4; i++) pthread_create(&t[i], 0, thr, (void *)(i + 10));
		for (int i = 0; i < 4; i++) {
			void *r;
			pthread_join(t[i], &r);
			CHECK(r == 0, "thread dynamic TLS %d", i);
		}
		CHECK(tls_get() == 99, "main TLS unaffected");
		void *h2 = dlopen("libplug.so", RTLD_NOW | RTLD_GLOBAL);
		CHECK(h2 == h, "dlopen same handle");
		CHECK(dlsym(RTLD_DEFAULT, "plug_tls_get") != 0, "RTLD_GLOBAL promotion");
		int (*ps)(const char *) = (int (*)(const char *))dlsym(h, "plug_strlen");
		int before = strlen_calls;
		CHECK(ps("abcd") == 4 && strlen_calls == before + 1, "interposition into plugin");
		CHECK(dlclose(h) == 0 && dlclose(h2) == 0, "dlclose");
	}
	void *self = dlopen(0, RTLD_NOW);
	CHECK(self && dlsym(self, "prog_exported") == (void *)prog_exported, "dlopen(NULL)");
	CHECK(dlsym(self, "printf") != 0, "global lookup from program handle");
	printf("%s: %d failures\n", __FILE__, fails);
	return fails != 0;
}
