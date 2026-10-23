/* lib-spfxd test — mutex protocols: robust mutexes (owner death through
 * thread exit, detached exit, death while others wait, process death of a
 * process-shared mutex, consistent/unrecoverable states, condition
 * variables), priority inheritance (contention, trylock, timeouts on both
 * clocks, robust+PI) and priority protection (ceiling boost, nesting,
 * EINVAL above the ceiling, changing the ceiling).
 *
 * The expectations follow POSIX.  glibc differs in two places, so this
 * test does not pass against it: pthread_mutex_trylock by the owner of a
 * robust or PI mutex returns EDEADLK there (POSIX: EBUSY), and unlocking
 * an inconsistent robust mutex wakes only one waiter, which leaves the
 * other waiters of the now unrecoverable mutex blocked forever. */
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "t.h"

static int emulated;

static void mkmutex(pthread_mutex_t *m, int type, int robust, int proto, int pshared)
{
	pthread_mutexattr_t a;
	pthread_mutexattr_init(&a);
	pthread_mutexattr_settype(&a, type);
	pthread_mutexattr_setrobust(&a, robust ? PTHREAD_MUTEX_ROBUST : PTHREAD_MUTEX_STALLED);
	pthread_mutexattr_setprotocol(&a, proto);
	pthread_mutexattr_setpshared(&a, pshared);
	CHECK_EQ_INT(pthread_mutex_init(m, &a), 0);
	pthread_mutexattr_destroy(&a);
}

static struct timespec in_ms(clockid_t clk, long ms)
{
	struct timespec t;
	clock_gettime(clk, &t);
	t.tv_sec += ms / 1000;
	t.tv_nsec += (ms % 1000) * 1000000;
	if (t.tv_nsec >= 1000000000) { t.tv_nsec -= 1000000000; t.tv_sec++; }
	return t;
}

/* ------------------------------------------------------- attributes */

static void test_attrs(void)
{
	pthread_mutexattr_t a;
	int v;
	pthread_mutexattr_init(&a);
	CHECK(!pthread_mutexattr_getrobust(&a, &v) && v == PTHREAD_MUTEX_STALLED, "default robust");
	CHECK(!pthread_mutexattr_getprotocol(&a, &v) && v == PTHREAD_PRIO_NONE, "default protocol");
	CHECK_EQ_INT(pthread_mutexattr_setrobust(&a, PTHREAD_MUTEX_ROBUST), 0);
	CHECK(!pthread_mutexattr_getrobust(&a, &v) && v == PTHREAD_MUTEX_ROBUST, "robust round trip");
	CHECK_EQ_INT(pthread_mutexattr_setrobust(&a, 7), EINVAL);
	CHECK_EQ_INT(pthread_mutexattr_setprotocol(&a, PTHREAD_PRIO_INHERIT), 0);
	CHECK(!pthread_mutexattr_getprotocol(&a, &v) && v == PTHREAD_PRIO_INHERIT, "PI round trip");
	CHECK_EQ_INT(pthread_mutexattr_setprotocol(&a, PTHREAD_PRIO_PROTECT), 0);
	CHECK(!pthread_mutexattr_getprotocol(&a, &v) && v == PTHREAD_PRIO_PROTECT, "PP round trip");
	CHECK_EQ_INT(pthread_mutexattr_setprotocol(&a, 9), EINVAL);
	CHECK_EQ_INT(pthread_mutexattr_setprioceiling(&a, 42), 0);
	CHECK(!pthread_mutexattr_getprioceiling(&a, &v) && v == 42, "ceiling round trip");
	CHECK_EQ_INT(pthread_mutexattr_setprioceiling(&a, 0), EINVAL);
	CHECK_EQ_INT(pthread_mutexattr_setprioceiling(&a, 100), EINVAL);
	int t;
	CHECK(!pthread_mutexattr_gettype(&a, &t) && t == PTHREAD_MUTEX_DEFAULT, "type unaffected");
	pthread_mutexattr_destroy(&a);
}

/* ---------------------------------------------------------- robust */

static pthread_mutex_t rm;
static void *lock_and_exit(void *arg)
{
	pthread_mutex_t *m = arg;
	if (pthread_mutex_lock(m)) return (void *)1;
	return 0;                               /* exits holding it */
}

static void *lock_and_die_later(void *arg)
{
	pthread_mutex_t *m = arg;
	if (pthread_mutex_lock(m)) return (void *)1;
	usleep(100000);                         /* let a waiter block */
	return 0;
}

static atomic_int waiter_result = -1;
static void *wait_for(void *arg)
{
	waiter_result = pthread_mutex_lock((pthread_mutex_t *)arg);
	if (waiter_result == EOWNERDEAD) {
		pthread_mutex_consistent((pthread_mutex_t *)arg);
		pthread_mutex_unlock((pthread_mutex_t *)arg);
	} else if (waiter_result == 0) {
		pthread_mutex_unlock((pthread_mutex_t *)arg);
	}
	return 0;
}

static void test_robust_type(int type, int proto)
{
	pthread_t t;
	void *r;
	mkmutex(&rm, type, 1, proto, 0);

	/* uncontended use is unchanged */
	CHECK_EQ_INT(pthread_mutex_lock(&rm), 0);
	CHECK_EQ_INT(pthread_mutex_trylock(&rm), type == PTHREAD_MUTEX_RECURSIVE ? 0 : EBUSY);
	if (type == PTHREAD_MUTEX_RECURSIVE) CHECK_EQ_INT(pthread_mutex_unlock(&rm), 0);
	CHECK_EQ_INT(pthread_mutex_unlock(&rm), 0);
	CHECK_EQ_INT(pthread_mutex_unlock(&rm), EPERM);

	/* owner exits holding it (joinable) */
	pthread_create(&t, 0, lock_and_exit, &rm);
	pthread_join(t, &r);
	CHECK(r == 0, "thread locked");
	CHECK_EQ_INT(pthread_mutex_lock(&rm), EOWNERDEAD);
	CHECK_EQ_INT(pthread_mutex_consistent(&rm), 0);
	CHECK_EQ_INT(pthread_mutex_consistent(&rm), EINVAL);   /* already consistent */
	CHECK_EQ_INT(pthread_mutex_unlock(&rm), 0);
	CHECK_EQ_INT(pthread_mutex_lock(&rm), 0);
	CHECK_EQ_INT(pthread_mutex_unlock(&rm), 0);

	/* owner exits while another thread is blocked on it */
	waiter_result = -1;
	pthread_t w;
	pthread_create(&t, 0, lock_and_die_later, &rm);
	usleep(20000);
	pthread_create(&w, 0, wait_for, &rm);
	pthread_join(t, 0);
	pthread_join(w, 0);
	CHECK_EQ_INT(waiter_result, EOWNERDEAD);
	CHECK_EQ_INT(pthread_mutex_lock(&rm), 0);
	CHECK_EQ_INT(pthread_mutex_unlock(&rm), 0);

	/* trylock also reports the dead owner */
	pthread_create(&t, 0, lock_and_exit, &rm);
	pthread_join(t, 0);
	CHECK_EQ_INT(pthread_mutex_trylock(&rm), EOWNERDEAD);
	/* unlocked while inconsistent: unrecoverable for good */
	CHECK_EQ_INT(pthread_mutex_unlock(&rm), 0);
	CHECK_EQ_INT(pthread_mutex_lock(&rm), ENOTRECOVERABLE);
	CHECK_EQ_INT(pthread_mutex_trylock(&rm), ENOTRECOVERABLE);
	CHECK_EQ_INT(pthread_mutex_destroy(&rm), 0);
}

/* a detached thread unmaps its own stack and TCB before it exits */
static atomic_int detached_done;
static void *detached_locker(void *arg)
{
	pthread_mutex_lock(arg);
	detached_done = 1;
	return 0;
}

static void test_robust_detached(void)
{
	pthread_attr_t a;
	pthread_t t;
	mkmutex(&rm, PTHREAD_MUTEX_NORMAL, 1, PTHREAD_PRIO_NONE, 0);
	pthread_attr_init(&a);
	pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
	detached_done = 0;
	pthread_create(&t, &a, detached_locker, &rm);
	while (!detached_done) usleep(1000);
	struct timespec dl = in_ms(CLOCK_REALTIME, 5000);
	CHECK_EQ_INT(pthread_mutex_timedlock(&rm, &dl), EOWNERDEAD);
	pthread_mutex_consistent(&rm);
	pthread_mutex_unlock(&rm);
	pthread_attr_destroy(&a);
}

/* every waiter wakes with ENOTRECOVERABLE */
static atomic_int nr_count;
static void *expect_unrecoverable(void *arg)
{
	if (pthread_mutex_lock(arg) == ENOTRECOVERABLE) nr_count++;
	return 0;
}

static void test_unrecoverable_wakes_all(int proto)
{
	pthread_t t[4], o;
	mkmutex(&rm, PTHREAD_MUTEX_NORMAL, 1, proto, 0);
	pthread_create(&o, 0, lock_and_exit, &rm);
	pthread_join(o, 0);
	CHECK_EQ_INT(pthread_mutex_lock(&rm), EOWNERDEAD);
	nr_count = 0;
	for (int i = 0; i < 4; i++) pthread_create(&t[i], 0, expect_unrecoverable, &rm);
	usleep(50000);
	pthread_mutex_unlock(&rm);              /* without consistent */
	for (int i = 0; i < 4; i++) pthread_join(t[i], 0);
	CHECK_EQ_INT(nr_count, 4);
}

/* condition variables return the re-acquisition result */
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static atomic_int cv_state, cv_result = -1;
static void *cv_waiter(void *arg)
{
	pthread_mutex_lock(&rm);
	cv_state = 1;
	cv_result = pthread_cond_wait(&cv, &rm);
	if (cv_result == EOWNERDEAD) pthread_mutex_consistent(&rm);
	pthread_mutex_unlock(&rm);
	return 0;
}

static void test_robust_cond(void)
{
	pthread_t w, o;
	mkmutex(&rm, PTHREAD_MUTEX_NORMAL, 1, PTHREAD_PRIO_NONE, 0);
	cv_state = 0;
	pthread_create(&w, 0, cv_waiter, &rm);
	while (!cv_state) usleep(1000);
	pthread_mutex_lock(&rm);                /* the waiter is in cond_wait */
	pthread_mutex_unlock(&rm);
	pthread_create(&o, 0, lock_and_exit, &rm);
	pthread_join(o, 0);                     /* dies holding the mutex */
	pthread_cond_signal(&cv);
	pthread_join(w, 0);
	CHECK_EQ_INT(cv_result, EOWNERDEAD);
}

/* process-shared robust mutex: the owning process dies (kernel path) */
static void test_robust_process(void)
{
	if (emulated) {
		SKIP("process death needs the kernel's robust list (qemu-user does not provide it)");
		return;
	}
	pthread_mutex_t *m = mmap(0, sizeof *m, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	CHECK(m != MAP_FAILED, "mmap");
	if (m == MAP_FAILED) return;
	for (int pi = 0; pi < 2; pi++) {
		mkmutex(m, PTHREAD_MUTEX_NORMAL, 1, pi ? PTHREAD_PRIO_INHERIT : PTHREAD_PRIO_NONE, 1);
		pid_t p = fork();
		if (p == 0) {
			pthread_mutex_lock(m);
			_exit(0);                       /* dies holding it */
		}
		int st;
		waitpid(p, &st, 0);
		struct timespec dl = in_ms(CLOCK_REALTIME, 5000);
		CHECK_EQ_INT(pthread_mutex_timedlock(m, &dl), EOWNERDEAD);
		CHECK_EQ_INT(pthread_mutex_consistent(m), 0);
		CHECK_EQ_INT(pthread_mutex_unlock(m), 0);
	}
	munmap(m, sizeof *m);
}

/* ------------------------------------------------- priority inheritance */

static pthread_mutex_t pm;
static long counter;
static void *pi_worker(void *arg)
{
	for (int i = 0; i < 20000; i++) {
		if (pthread_mutex_lock(&pm)) return (void *)1;
		counter++;
		if (pthread_mutex_unlock(&pm)) return (void *)2;
	}
	return 0;
}

static void *pi_holder(void *arg)
{
	pthread_mutex_lock(&pm);
	*(atomic_int *)arg = 1;
	usleep(300000);
	pthread_mutex_unlock(&pm);
	return 0;
}

static void test_pi(void)
{
	mkmutex(&pm, PTHREAD_MUTEX_ERRORCHECK, 0, PTHREAD_PRIO_INHERIT, 0);
	int r = pthread_mutex_lock(&pm);
	if (r == ENOSYS && emulated) {
		SKIP("PI futexes not provided by the emulator");
		return;
	}
	CHECK_EQ_INT(r, 0);
	CHECK_EQ_INT(pthread_mutex_lock(&pm), EDEADLK);
	CHECK_EQ_INT(pthread_mutex_trylock(&pm), EBUSY);
	CHECK_EQ_INT(pthread_mutex_unlock(&pm), 0);
	CHECK_EQ_INT(pthread_mutex_unlock(&pm), EPERM);

	/* contention: the kernel path (FUTEX_LOCK_PI / UNLOCK_PI) */
	pthread_t t[4];
	void *res;
	counter = 0;
	for (int i = 0; i < 4; i++) pthread_create(&t[i], 0, pi_worker, 0);
	int ok = 1;
	for (int i = 0; i < 4; i++) { pthread_join(t[i], &res); ok &= res == 0; }
	CHECK(ok && counter == 80000, "PI contention: counter %ld", counter);

	/* trylock and timeouts while another thread holds it */
	atomic_int held = 0;
	pthread_t h;
	pthread_create(&h, 0, pi_holder, &held);
	while (!held) usleep(1000);
	CHECK_EQ_INT(pthread_mutex_trylock(&pm), EBUSY);
	struct timespec dl = in_ms(CLOCK_REALTIME, 30);
	CHECK_EQ_INT(pthread_mutex_timedlock(&pm, &dl), ETIMEDOUT);
	dl = in_ms(CLOCK_MONOTONIC, 30);
	CHECK_EQ_INT(pthread_mutex_clocklock(&pm, CLOCK_MONOTONIC, &dl), ETIMEDOUT);
	dl = in_ms(CLOCK_MONOTONIC, 5000);
	CHECK_EQ_INT(pthread_mutex_clocklock(&pm, CLOCK_MONOTONIC, &dl), 0);
	CHECK_EQ_INT(pthread_mutex_unlock(&pm), 0);
	pthread_join(h, 0);

	/* recursive PI */
	mkmutex(&pm, PTHREAD_MUTEX_RECURSIVE, 0, PTHREAD_PRIO_INHERIT, 0);
	CHECK(!pthread_mutex_lock(&pm) && !pthread_mutex_lock(&pm) && !pthread_mutex_trylock(&pm), "recursive PI");
	CHECK(!pthread_mutex_unlock(&pm) && !pthread_mutex_unlock(&pm) && !pthread_mutex_unlock(&pm), "recursive PI unlock");
	CHECK_EQ_INT(pthread_mutex_unlock(&pm), EPERM);

	/* robust + PI */
	test_robust_type(PTHREAD_MUTEX_NORMAL, PTHREAD_PRIO_INHERIT);
	test_unrecoverable_wakes_all(PTHREAD_PRIO_INHERIT);
}

/* ------------------------------------------------- priority protection */

static int my_prio(void)
{
	struct sched_param sp;
	sched_getparam(0, &sp);
	return sp.sched_priority;
}

static void test_pp(void)
{
	pthread_mutex_t a, b;
	pthread_mutexattr_t at;
	pthread_mutexattr_init(&at);
	pthread_mutexattr_setprotocol(&at, PTHREAD_PRIO_PROTECT);
	pthread_mutexattr_setprioceiling(&at, 20);
	pthread_mutex_init(&a, &at);
	pthread_mutexattr_setprioceiling(&at, 40);
	pthread_mutex_init(&b, &at);
	pthread_mutexattr_destroy(&at);
	int c, old;
	CHECK(!pthread_mutex_getprioceiling(&a, &c) && c == 20, "mutex ceiling");

	/* any policy: locking works */
	CHECK(!pthread_mutex_lock(&a) && !pthread_mutex_unlock(&a), "PP lock under default policy");

	struct sched_param sp = { .sched_priority = 10 };
	if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp)) {
		SKIP("SCHED_FIFO not permitted: ceiling boost not tested");
	} else {
		CHECK_EQ_INT(my_prio(), 10);
		CHECK_EQ_INT(pthread_mutex_lock(&a), 0);
		CHECK_EQ_INT(my_prio(), 20);
		CHECK_EQ_INT(pthread_mutex_lock(&b), 0);
		CHECK_EQ_INT(my_prio(), 40);
		CHECK_EQ_INT(pthread_mutex_unlock(&b), 0);
		CHECK_EQ_INT(my_prio(), 20);
		CHECK_EQ_INT(pthread_mutex_unlock(&a), 0);
		CHECK_EQ_INT(my_prio(), 10);
		/* setschedprio while boosted changes the base */
		pthread_mutex_lock(&a);
		CHECK_EQ_INT(pthread_setschedprio(pthread_self(), 15), 0);
		CHECK_EQ_INT(my_prio(), 20);
		pthread_mutex_unlock(&a);
		CHECK_EQ_INT(my_prio(), 15);
		/* above the ceiling: EINVAL */
		sp.sched_priority = 30;
		pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
		CHECK_EQ_INT(pthread_mutex_lock(&a), EINVAL);
		CHECK_EQ_INT(pthread_mutex_trylock(&a), EINVAL);
		CHECK_EQ_INT(my_prio(), 30);
		/* raise the ceiling (taken without the old one), then lock */
		CHECK(!pthread_mutex_setprioceiling(&a, 50, &old) && old == 20, "setprioceiling");
		CHECK_EQ_INT(pthread_mutex_lock(&a), 0);
		CHECK_EQ_INT(my_prio(), 50);
		pthread_mutex_unlock(&a);
		CHECK_EQ_INT(my_prio(), 30);
		sp.sched_priority = 0;
		pthread_setschedparam(pthread_self(), SCHED_OTHER, &sp);
	}
	CHECK_EQ_INT(pthread_mutex_setprioceiling(&a, 0, &old), EINVAL);
	pthread_mutex_t plain = PTHREAD_MUTEX_INITIALIZER;
	CHECK_EQ_INT(pthread_mutex_getprioceiling(&plain, &c), EINVAL);
}

int main(void)
{
	emulated = getenv("SPFXD_EMULATED") != 0;
	test_attrs();
	test_robust_type(PTHREAD_MUTEX_NORMAL, PTHREAD_PRIO_NONE);
	test_robust_type(PTHREAD_MUTEX_ERRORCHECK, PTHREAD_PRIO_NONE);
	test_robust_type(PTHREAD_MUTEX_RECURSIVE, PTHREAD_PRIO_NONE);
	test_robust_detached();
	test_unrecoverable_wakes_all(PTHREAD_PRIO_NONE);
	test_robust_cond();
	test_robust_process();
	test_pi();
	test_pp();
	return DONE();
}
