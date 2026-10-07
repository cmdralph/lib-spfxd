/* lib-spfxd — C11 <threads.h> on top of the pthread runtime. */
#include <errno.h>
#include <stdlib.h>
#include <threads.h>
#include <pthread.h>
#include <time.h>
#include <stdint.h>
#include "pthread_impl.h"

static int map_err(int r)
{
	switch (r) {
	case 0: return thrd_success;
	case EBUSY: return thrd_busy;
	case ETIMEDOUT: return thrd_timedout;
	case ENOMEM: case EAGAIN: return thrd_nomem;
	default: return thrd_error;
	}
}

struct c11_start { thrd_start_t fn; void *arg; };

static void *c11_trampoline(void *p)
{
	struct c11_start s = *(struct c11_start *)p;
	free(p);
	return (void *)(intptr_t)s.fn(s.arg);
}

int thrd_create(thrd_t *t, thrd_start_t fn, void *arg)
{
	struct c11_start *s = malloc(sizeof *s);
	if (!s) return thrd_nomem;
	s->fn = fn;
	s->arg = arg;
	int r = pthread_create(t, 0, c11_trampoline, s);
	if (r) free(s);
	return map_err(r);
}

int thrd_equal(thrd_t a, thrd_t b) { return a == b; }
thrd_t thrd_current(void) { return pthread_self(); }
void thrd_yield(void) { __syscall(SYS_sched_yield); }
_Noreturn void thrd_exit(int r) { pthread_exit((void *)(intptr_t)r); }
int thrd_detach(thrd_t t) { return map_err(pthread_detach(t)); }

int thrd_join(thrd_t t, int *res)
{
	void *p;
	int r = pthread_join(t, &p);
	if (!r && res) *res = (int)(intptr_t)p;
	return map_err(r);
}

int thrd_sleep(const struct timespec *req, struct timespec *rem)
{
	long r = -__syscall_cp(SYS_nanosleep, req, rem);
	if (!r) return 0;
	return r == EINTR ? -1 : -2;
}

int mtx_init(mtx_t *m, int type)
{
	pthread_mutexattr_t a;
	pthread_mutexattr_init(&a);
	if (type & mtx_recursive) pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
	return map_err(pthread_mutex_init(m, &a));
}

void mtx_destroy(mtx_t *m) { pthread_mutex_destroy(m); }
int mtx_lock(mtx_t *m) { return map_err(pthread_mutex_lock(m)); }
int mtx_trylock(mtx_t *m) { return map_err(pthread_mutex_trylock(m)); }
int mtx_unlock(mtx_t *m) { return map_err(pthread_mutex_unlock(m)); }

int mtx_timedlock(mtx_t *restrict m, const struct timespec *restrict ts)
{
	return map_err(pthread_mutex_timedlock(m, ts));
}

void call_once(once_flag *f, void (*fn)(void)) { pthread_once(f, fn); }

int cnd_init(cnd_t *c) { return map_err(pthread_cond_init(c, 0)); }
void cnd_destroy(cnd_t *c) { pthread_cond_destroy(c); }
int cnd_signal(cnd_t *c) { return map_err(pthread_cond_signal(c)); }
int cnd_broadcast(cnd_t *c) { return map_err(pthread_cond_broadcast(c)); }
int cnd_wait(cnd_t *c, mtx_t *m) { return map_err(pthread_cond_wait(c, m)); }

int cnd_timedwait(cnd_t *restrict c, mtx_t *restrict m, const struct timespec *restrict ts)
{
	return map_err(pthread_cond_timedwait(c, m, ts));
}

int tss_create(tss_t *k, tss_dtor_t d) { return map_err(pthread_key_create(k, d)); }
void tss_delete(tss_t k) { pthread_key_delete(k); }
int tss_set(tss_t k, void *v) { return map_err(pthread_setspecific(k, v)); }
void *tss_get(tss_t k) { return pthread_getspecific(k); }
