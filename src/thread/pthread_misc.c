/* lib-spfxd — spinlocks, once, barriers, keys (thread-specific data). */
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "pthread_impl.h"

/* ---- spinlocks ---- */

int pthread_spin_init(pthread_spinlock_t *s, int shared) { *s = 0; return 0; }
int pthread_spin_destroy(pthread_spinlock_t *s) { return 0; }

int pthread_spin_lock(pthread_spinlock_t *s)
{
	while (*s || a_cas(s, 0, EBUSY)) a_spin();
	return 0;
}

int pthread_spin_trylock(pthread_spinlock_t *s)
{
	return a_cas(s, 0, EBUSY);
}

int pthread_spin_unlock(pthread_spinlock_t *s)
{
	a_store_rel(s, 0);
	return 0;
}

/* ---- once: 0 = not run, 1 = running, 3 = running with waiters, 2 = done ---- */

static void once_undo(void *p)
{
	volatile int *c = p;
	if (a_swap(c, 0) == 3) __futex_wake(c, INT_MAX, 1);
}

int pthread_once(pthread_once_t *c, void (*init)(void))
{
	if (a_load(c) == 2) return 0;
	for (;;) {
		int v = a_cas(c, 0, 1);
		if (v == 0) {
			struct __spfxd_cleanup cb;
			__spfxd_cleanup_push(&cb, once_undo, (void *)c);
			init();
			__spfxd_cleanup_pop(&cb, 0);
			if (a_swap(c, 2) == 3) __futex_wake(c, INT_MAX, 1);
			return 0;
		}
		if (v == 2) return 0;
		if (v == 1) a_cas(c, 1, 3);
		__futex_wait(c, 3, 1);
	}
}

/* ---- barriers ---- */

int pthread_barrierattr_init(pthread_barrierattr_t *a) { a->__attr = 0; return 0; }
int pthread_barrierattr_destroy(pthread_barrierattr_t *a) { return 0; }

int pthread_barrierattr_getpshared(const pthread_barrierattr_t *restrict a, int *restrict p)
{
	*p = (int)a->__attr;
	return 0;
}

int pthread_barrierattr_setpshared(pthread_barrierattr_t *a, int p)
{
	if ((unsigned)p > 1) return EINVAL;
	a->__attr = (unsigned)p;
	return 0;
}

int pthread_barrier_init(pthread_barrier_t *restrict b, const pthread_barrierattr_t *restrict a, unsigned count)
{
	if (!count || count > INT_MAX) return EINVAL;
	memset(b, 0, sizeof *b);
	b->__limit = (int)count;
	if (a) b->__shared = (int)a->__attr;
	return 0;
}

/* __pad[0] counts threads still inside pthread_barrier_wait, so destroy
 * can wait until no thread will touch the object again. */
int pthread_barrier_destroy(pthread_barrier_t *b)
{
	while (a_load(&b->__pad[0])) a_spin();
	return 0;
}

int pthread_barrier_wait(pthread_barrier_t *b)
{
	int priv = !b->__shared;
	a_inc(&b->__pad[0]);
	/* tiny critical section; a spin lock also works for process-shared
	 * barriers without futex address-space concerns */
	while (a_cas(&b->__lock, 0, 1)) a_spin();
	unsigned gen = b->__gen;
	if (++b->__count == b->__limit) {
		b->__count = 0;
		a_inc((volatile int *)&b->__gen);
		a_store(&b->__lock, 0);
		__futex_wake(&b->__gen, INT_MAX, priv);
		a_dec(&b->__pad[0]);
		return PTHREAD_BARRIER_SERIAL_THREAD;
	}
	a_store(&b->__lock, 0);
	while (a_load((volatile int *)&b->__gen) == (int)gen)
		__futex_wait(&b->__gen, (int)gen, priv);
	a_dec(&b->__pad[0]);
	return 0;
}

/* ---- thread-specific data ---- */

static void (*key_dtors[PTHREAD_KEYS_MAX])(void *);
static volatile int key_lock;

static void no_dtor(void *p) { }

int pthread_key_create(pthread_key_t *k, void (*dtor)(void *))
{
	if (!dtor) dtor = no_dtor;
	__lock_always(&key_lock);
	for (unsigned i = 0; i < PTHREAD_KEYS_MAX; i++) {
		if (!key_dtors[i]) {
			key_dtors[i] = dtor;
			__unlock_always(&key_lock);
			*k = i;
			return 0;
		}
	}
	__unlock_always(&key_lock);
	return EAGAIN;
}

int pthread_key_delete(pthread_key_t k)
{
	if (k >= PTHREAD_KEYS_MAX) return EINVAL;
	sigset_t set;
	__block_app_sigs(&set);
	__tl_lock();
	struct pthread *self = __self(), *t = self;
	do t->tsd[k] = 0; while ((t = t->next) != self);
	__lock_always(&key_lock);
	key_dtors[k] = 0;
	__unlock_always(&key_lock);
	__tl_unlock();
	__restore_sigs(&set);
	return 0;
}

void *pthread_getspecific(pthread_key_t k)
{
	return __self()->tsd[k];
}

int pthread_setspecific(pthread_key_t k, const void *v)
{
	if (k >= PTHREAD_KEYS_MAX) return EINVAL;
	struct pthread *self = __self();
	self->tsd[k] = (void *)v;
	if (v) self->tsd_used = 1;
	return 0;
}

hidden void __pthread_tsd_run_dtors(void)
{
	struct pthread *self = __self();
	for (int it = 0; it < PTHREAD_DESTRUCTOR_ITERATIONS && self->tsd_used; it++) {
		self->tsd_used = 0;
		for (unsigned i = 0; i < PTHREAD_KEYS_MAX; i++) {
			void *v = self->tsd[i];
			void (*d)(void *) = key_dtors[i];
			if (v && d && d != no_dtor) {
				self->tsd[i] = 0;
				d(v);
			}
		}
	}
}
