/* lib-spfxd test — threads: creation, joining, detaching, TLS, mutexes of
 * every type, condition variables (with timeouts), rwlocks, barriers,
 * spinlocks, once, keys, cancellation, semaphores, C11 threads. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>
#include "t.h"

static __thread int tls_var = 5;
static __thread char tls_big[10000];

static void *ret_arg(void *a) { return a; }

static void *tls_thread(void *a)
{
	if (tls_var != 5 || tls_big[9999]) return (void *)1;
	tls_var = (int)(long)a;
	tls_big[9999] = 1;
	errno = (int)(long)a;
	sched_yield();
	return (void *)(long)(tls_var == (int)(long)a && errno == (int)(long)a ? 0 : 2);
}

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static long counter;
static void *inc(void *a)
{
	for (int i = 0; i < 100000; i++) {
		pthread_mutex_lock(&m);
		counter++;
		pthread_mutex_unlock(&m);
	}
	return a;
}

static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int ready, consumed;
static void *waiter(void *a)
{
	pthread_mutex_lock(&m);
	while (!ready) pthread_cond_wait(&cv, &m);
	consumed++;
	pthread_mutex_unlock(&m);
	return a;
}

static pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
static atomic_int readers_in, max_readers;
static void *reader(void *a)
{
	for (int i = 0; i < 2000; i++) {
		pthread_rwlock_rdlock(&rw);
		int n = atomic_fetch_add(&readers_in, 1) + 1;
		int mx = atomic_load(&max_readers);
		while (n > mx && !atomic_compare_exchange_weak(&max_readers, &mx, n));
		atomic_fetch_sub(&readers_in, 1);
		pthread_rwlock_unlock(&rw);
	}
	return a;
}
static void *writer(void *a)
{
	for (int i = 0; i < 2000; i++) {
		pthread_rwlock_wrlock(&rw);
		if (atomic_load(&readers_in)) { pthread_rwlock_unlock(&rw); return (void *)1; }
		counter++;
		pthread_rwlock_unlock(&rw);
	}
	return a;
}

static pthread_barrier_t bar;
static atomic_int phase_count, serial;
static void *barrier_thread(void *a)
{
	for (int p = 0; p < 50; p++) {
		atomic_fetch_add(&phase_count, 1);
		if (pthread_barrier_wait(&bar) == PTHREAD_BARRIER_SERIAL_THREAD) atomic_fetch_add(&serial, 1);
	}
	return a;
}

static pthread_once_t once = PTHREAD_ONCE_INIT;
static int once_runs;
static void once_fn(void) { once_runs++; }
static void *once_thread(void *a) { pthread_once(&once, once_fn); return a; }

static pthread_key_t key;
static atomic_int dtor_runs;
static void key_dtor(void *v) { if (v) atomic_fetch_add(&dtor_runs, 1); }
static void *key_thread(void *a) { pthread_setspecific(key, a); return pthread_getspecific(key) == a ? 0 : (void *)1; }

static atomic_int cleanup_ran;
static void cleanup(void *a) { atomic_store(&cleanup_ran, (int)(long)a); }
static void *cancel_me(void *a)
{
	pthread_cleanup_push(cleanup, (void *)7);
	for (;;) pause();                        /* pause is a cancellation point */
	pthread_cleanup_pop(0);
	return a;
}
static void *cancel_disabled(void *a)
{
	int old;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
	usleep(50000);
	pthread_setcancelstate(old, 0);
	pthread_testcancel();
	return (void *)99;                        /* not reached */
}
static volatile int spin_forever = 1;
static void *async_cancel(void *a)
{
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
	while (spin_forever);
	return a;
}

static sem_t sem;
static void *sem_poster(void *a) { usleep(20000); sem_post(&sem); return a; }

static int c11_fn(void *a) { return (int)(long)a + 1; }
static mtx_t cm;
static cnd_t cc;
static int c11_flag;
static int c11_waiter(void *a)
{
	mtx_lock(&cm);
	while (!c11_flag) cnd_wait(&cc, &cm);
	mtx_unlock(&cm);
	return (int)(long)a;
}

/* Reused thread stacks (the stack cache) must look fresh: initialized
 * and zero-initialized TLS, errno and thread-specific data. */
static _Thread_local int tls_init_42 = 42;
static _Thread_local long tls_zero;
static pthread_key_t fresh_key;

static void *fresh_thread(void *arg)
{
	int ok = tls_init_42 == 42 && tls_zero == 0 && errno == 0 &&
	         pthread_getspecific(fresh_key) == 0;
	tls_init_42 = 7;
	tls_zero = 99;
	errno = 1234;
	pthread_setspecific(fresh_key, (void *)arg);
	return (void *)(long)ok;
}

int main(void)
{
	void *r;
	pthread_t t[8];
	/* create/join, return values, self/equal */
	CHECK(pthread_create(&t[0], 0, ret_arg, (void *)42) == 0, "create");
	CHECK(pthread_join(t[0], &r) == 0 && r == (void *)42, "join value");
	CHECK(pthread_equal(pthread_self(), pthread_self()), "self");
	/* TLS and errno per thread */
	for (long i = 0; i < 8; i++) pthread_create(&t[i], 0, tls_thread, (void *)(i + 10));
	for (int i = 0; i < 8; i++) { pthread_join(t[i], &r); CHECK(r == 0, "TLS thread %d: %p", i, r); }
	CHECK(tls_var == 5 && !tls_big[9999], "main thread TLS untouched");
	/* mutex contention */
	counter = 0;
	for (int i = 0; i < 4; i++) pthread_create(&t[i], 0, inc, 0);
	for (int i = 0; i < 4; i++) pthread_join(t[i], 0);
	CHECK(counter == 400000, "mutex counter %ld", counter);
	/* mutex types */
	pthread_mutexattr_t ma;
	pthread_mutex_t rm, em;
	pthread_mutexattr_init(&ma);
	pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&rm, &ma);
	CHECK(!pthread_mutex_lock(&rm) && !pthread_mutex_lock(&rm) && !pthread_mutex_unlock(&rm) &&
	      !pthread_mutex_unlock(&rm) && pthread_mutex_unlock(&rm) == EPERM, "recursive mutex");
	pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(&em, &ma);
	CHECK(!pthread_mutex_lock(&em) && pthread_mutex_lock(&em) == EDEADLK && !pthread_mutex_unlock(&em) &&
	      pthread_mutex_unlock(&em) == EPERM, "errorcheck mutex");
	CHECK(!pthread_mutex_trylock(&em) && pthread_mutex_trylock(&em) == EBUSY, "trylock");
	pthread_mutex_unlock(&em);
	struct timespec ts;
	pthread_mutex_lock(&m);
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_nsec += 20000000;
	if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
	CHECK(pthread_mutex_timedlock(&em, &ts) == 0, "timedlock free mutex");
	pthread_mutex_unlock(&em);
	pthread_mutex_unlock(&m);
	/* condition variables */
	for (int i = 0; i < 4; i++) pthread_create(&t[i], 0, waiter, 0);
	usleep(20000);
	pthread_mutex_lock(&m);
	ready = 1;
	pthread_cond_broadcast(&cv);
	pthread_mutex_unlock(&m);
	for (int i = 0; i < 4; i++) pthread_join(t[i], 0);
	CHECK(consumed == 4, "broadcast wakes all");
	pthread_mutex_lock(&m);
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_nsec += 30000000;
	if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
	CHECK(pthread_cond_timedwait(&cv, &m, &ts) == ETIMEDOUT, "cond timedwait times out");
	pthread_mutex_unlock(&m);
	pthread_condattr_t ca;
	pthread_cond_t mcv;
	pthread_condattr_init(&ca);
	pthread_condattr_setclock(&ca, CLOCK_MONOTONIC);
	pthread_cond_init(&mcv, &ca);
	pthread_mutex_lock(&m);
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_nsec += 10000000;
	if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
	CHECK(pthread_cond_timedwait(&mcv, &m, &ts) == ETIMEDOUT, "monotonic cond timedwait");
	pthread_mutex_unlock(&m);
	/* rwlock */
	counter = 0;
	for (int i = 0; i < 4; i++) pthread_create(&t[i], 0, reader, 0);
	for (int i = 4; i < 6; i++) pthread_create(&t[i], 0, writer, 0);
	int rw_ok = 1;
	for (int i = 0; i < 6; i++) { pthread_join(t[i], &r); if (r) rw_ok = 0; }
	CHECK(rw_ok && counter == 4000, "rwlock exclusion (writers saw no readers)");
	CHECK(!pthread_rwlock_rdlock(&rw) && !pthread_rwlock_tryrdlock(&rw) && pthread_rwlock_trywrlock(&rw) == EBUSY,
	      "rwlock try");
	pthread_rwlock_unlock(&rw);
	pthread_rwlock_unlock(&rw);
	/* barrier */
	pthread_barrier_init(&bar, 0, 4);
	for (int i = 0; i < 4; i++) pthread_create(&t[i], 0, barrier_thread, 0);
	for (int i = 0; i < 4; i++) pthread_join(t[i], 0);
	CHECK(phase_count == 200 && serial == 50, "barrier phases %d serial %d", (int)phase_count, (int)serial);
	pthread_barrier_destroy(&bar);
	/* spinlock */
	pthread_spinlock_t sl;
	CHECK(!pthread_spin_init(&sl, 0) && !pthread_spin_lock(&sl) && pthread_spin_trylock(&sl) == EBUSY &&
	      !pthread_spin_unlock(&sl), "spinlock");
	/* once */
	for (int i = 0; i < 8; i++) pthread_create(&t[i], 0, once_thread, 0);
	for (int i = 0; i < 8; i++) pthread_join(t[i], 0);
	CHECK(once_runs == 1, "pthread_once");
	/* keys and destructors */
	CHECK(!pthread_key_create(&key, key_dtor), "key create");
	for (long i = 0; i < 4; i++) pthread_create(&t[i], 0, key_thread, (void *)(i + 1));
	for (int i = 0; i < 4; i++) { pthread_join(t[i], &r); CHECK(!r, "key value"); }
	CHECK(dtor_runs == 4, "key destructors ran %d", (int)dtor_runs);
	pthread_key_delete(key);
	/* cancellation */
	pthread_create(&t[0], 0, cancel_me, 0);
	usleep(20000);
	CHECK(!pthread_cancel(t[0]) && !pthread_join(t[0], &r) && r == PTHREAD_CANCELED && cleanup_ran == 7,
	      "deferred cancel + cleanup handler");
	pthread_create(&t[0], 0, cancel_disabled, 0);
	usleep(10000);
	pthread_cancel(t[0]);
	CHECK(!pthread_join(t[0], &r) && r == PTHREAD_CANCELED, "cancel acted on at testcancel");
	pthread_create(&t[0], 0, async_cancel, 0);
	usleep(20000);
	pthread_cancel(t[0]);
	CHECK(!pthread_join(t[0], &r) && r == PTHREAD_CANCELED, "asynchronous cancel");
	/* detach */
	pthread_attr_t at;
	pthread_attr_init(&at);
	pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
	CHECK(!pthread_create(&t[0], &at, ret_arg, 0), "detached create");
	pthread_attr_setdetachstate(&at, PTHREAD_CREATE_JOINABLE);
	CHECK(!pthread_attr_setstacksize(&at, 1 << 20) && !pthread_create(&t[0], &at, ret_arg, (void *)1) &&
	      !pthread_join(t[0], &r) && r == (void *)1, "custom stack size");
	size_t ss;
	pthread_attr_getstacksize(&at, &ss);
	CHECK(ss == 1 << 20, "getstacksize");
	CHECK(pthread_attr_setstacksize(&at, 100) == EINVAL, "stack below PTHREAD_STACK_MIN");
	pthread_attr_destroy(&at);
	CHECK(pthread_join(pthread_self(), 0) == EDEADLK, "join self");
	CHECK(!pthread_setname_np(pthread_self(), "spfxd-main"), "setname");
	char nm[16];
	CHECK(!pthread_getname_np(pthread_self(), nm, sizeof nm) && !strcmp(nm, "spfxd-main"), "getname");
	/* semaphores */
	CHECK(!sem_init(&sem, 0, 0), "sem_init");
	CHECK(sem_trywait(&sem) == -1 && errno == EAGAIN, "sem_trywait empty");
	pthread_create(&t[0], 0, sem_poster, 0);
	CHECK(!sem_wait(&sem), "sem_wait woken by post");
	pthread_join(t[0], 0);
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_nsec += 10000000;
	if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
	CHECK(sem_timedwait(&sem, &ts) == -1 && errno == ETIMEDOUT, "sem_timedwait");
	int sv;
	sem_post(&sem);
	CHECK(!sem_getvalue(&sem, &sv) && sv == 1, "sem_getvalue");
	sem_destroy(&sem);
	sem_t *ns = sem_open("/spfxd_test_sem", O_CREAT, 0600, 2);
	if (ns == SEM_FAILED) {
		SKIP("named semaphores unavailable (no /dev/shm)");
	} else {
		CHECK(!sem_wait(ns) && !sem_wait(ns) && sem_trywait(ns) == -1, "named semaphore");
		sem_close(ns);
		sem_unlink("/spfxd_test_sem");
	}
	/* C11 threads */
	thrd_t ct;
	int res;
	CHECK(thrd_create(&ct, c11_fn, (void *)41) == thrd_success && thrd_join(ct, &res) == thrd_success && res == 42,
	      "C11 thrd_create/join");
	mtx_init(&cm, mtx_plain);
	cnd_init(&cc);
	thrd_create(&ct, c11_waiter, (void *)5);
	thrd_sleep(&(struct timespec){ 0, 10000000 }, 0);
	mtx_lock(&cm);
	c11_flag = 1;
	cnd_signal(&cc);
	mtx_unlock(&cm);
	CHECK(thrd_join(ct, &res) == thrd_success && res == 5, "C11 mtx/cnd");
	tss_t tk;
	CHECK(tss_create(&tk, 0) == thrd_success && tss_set(tk, &res) == thrd_success && tss_get(tk) == &res, "C11 tss");
	once_flag of = ONCE_FLAG_INIT;
	call_once(&of, once_fn);
	CHECK(once_runs == 2, "call_once");
	/* many short-lived threads (stack/TLS reuse) */
	for (int round = 0; round < 200; round++) {
		CHECK(!pthread_create(&t[0], 0, tls_thread, (void *)(long)(round + 1)), "create %d", round);
		pthread_join(t[0], &r);
		if (r) { CHECK(0, "reused thread state round %d", round); break; }
	}
	{
		pthread_key_create(&fresh_key, 0);
		int fresh = 1;
		for (int i = 0; i < 50; i++) {
			pthread_t ft;
			void *ok;
			pthread_create(&ft, 0, fresh_thread, (void *)1);
			pthread_join(ft, &ok);
			fresh &= ok != 0;
		}
		CHECK(fresh, "threads on reused stacks start with fresh TLS, errno and TSD");
	}
	return DONE();
}
