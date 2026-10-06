/* lib-spfxd — <pthread.h> */
#ifndef _PTHREAD_H
#define _PTHREAD_H
#include <features.h>

#define __SPFXD_NEED_pthread_t
#define __SPFXD_NEED_pthread_types
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_clockid_t
#define __SPFXD_NEED_struct_timespec
#define __SPFXD_NEED_time_t
#define __SPFXD_NEED_sigset_t
#include <bits/typedefs.h>
#include <sched.h>
#include <time.h>

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#define PTHREAD_MUTEX_NORMAL     0
#define PTHREAD_MUTEX_DEFAULT    0
#define PTHREAD_MUTEX_RECURSIVE  1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_STALLED    0
#define PTHREAD_MUTEX_ROBUST     1

#define PTHREAD_PRIO_NONE    0
#define PTHREAD_PRIO_INHERIT 1
#define PTHREAD_PRIO_PROTECT 2

#define PTHREAD_INHERIT_SCHED  0
#define PTHREAD_EXPLICIT_SCHED 1
#define PTHREAD_SCOPE_SYSTEM   0
#define PTHREAD_SCOPE_PROCESS  1
#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED  1

#define PTHREAD_MUTEX_INITIALIZER {0}
#define PTHREAD_RWLOCK_INITIALIZER {0}
#define PTHREAD_COND_INITIALIZER {0}
#define PTHREAD_ONCE_INIT 0
#if defined(__SPFXD_GNU)
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP {0, 0, 0, PTHREAD_MUTEX_RECURSIVE}
#define PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP {0, 0, 0, PTHREAD_MUTEX_ERRORCHECK}
#endif

#define PTHREAD_CANCEL_ENABLE  0
#define PTHREAD_CANCEL_DISABLE 1
#define PTHREAD_CANCEL_DEFERRED 0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1
#define PTHREAD_CANCELED ((void *)-1)
#define PTHREAD_BARRIER_SERIAL_THREAD (-1)

__SPFXD_BEGIN_DECLS

int pthread_create(pthread_t *__restrict, const pthread_attr_t *__restrict, void *(*)(void *), void *__restrict);
int pthread_detach(pthread_t);
__spfxd_noreturn void pthread_exit(void *);
int pthread_join(pthread_t, void **);
pthread_t pthread_self(void) __spfxd_const;
int pthread_equal(pthread_t, pthread_t);
#ifndef __cplusplus
#define pthread_equal(a, b) ((a) == (b))
#endif
int pthread_once(pthread_once_t *, void (*)(void));
int pthread_atfork(void (*)(void), void (*)(void), void (*)(void));
int pthread_getconcurrency(void);
int pthread_setconcurrency(int);
int pthread_getcpuclockid(pthread_t, clockid_t *);
int pthread_getschedparam(pthread_t, int *__restrict, struct sched_param *__restrict);
int pthread_setschedparam(pthread_t, int, const struct sched_param *);
int pthread_setschedprio(pthread_t, int);

int pthread_cancel(pthread_t);
int pthread_setcancelstate(int, int *);
int pthread_setcanceltype(int, int *);
void pthread_testcancel(void);

int pthread_mutex_init(pthread_mutex_t *__restrict, const pthread_mutexattr_t *__restrict);
int pthread_mutex_destroy(pthread_mutex_t *);
int pthread_mutex_lock(pthread_mutex_t *);
int pthread_mutex_trylock(pthread_mutex_t *);
int pthread_mutex_unlock(pthread_mutex_t *);
int pthread_mutex_timedlock(pthread_mutex_t *__restrict, const struct timespec *__restrict);
int pthread_mutex_clocklock(pthread_mutex_t *__restrict, clockid_t, const struct timespec *__restrict);
int pthread_mutex_consistent(pthread_mutex_t *);
int pthread_mutex_getprioceiling(const pthread_mutex_t *__restrict, int *__restrict);
int pthread_mutex_setprioceiling(pthread_mutex_t *__restrict, int, int *__restrict);

int pthread_mutexattr_init(pthread_mutexattr_t *);
int pthread_mutexattr_destroy(pthread_mutexattr_t *);
int pthread_mutexattr_gettype(const pthread_mutexattr_t *__restrict, int *__restrict);
int pthread_mutexattr_settype(pthread_mutexattr_t *, int);
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *__restrict, int *__restrict);
int pthread_mutexattr_setpshared(pthread_mutexattr_t *, int);
int pthread_mutexattr_getrobust(const pthread_mutexattr_t *__restrict, int *__restrict);
int pthread_mutexattr_setrobust(pthread_mutexattr_t *, int);
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *__restrict, int *__restrict);
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *__restrict, int *__restrict);
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *, int);

int pthread_cond_init(pthread_cond_t *__restrict, const pthread_condattr_t *__restrict);
int pthread_cond_destroy(pthread_cond_t *);
int pthread_cond_wait(pthread_cond_t *__restrict, pthread_mutex_t *__restrict);
int pthread_cond_timedwait(pthread_cond_t *__restrict, pthread_mutex_t *__restrict, const struct timespec *__restrict);
int pthread_cond_clockwait(pthread_cond_t *__restrict, pthread_mutex_t *__restrict, clockid_t, const struct timespec *__restrict);
int pthread_cond_broadcast(pthread_cond_t *);
int pthread_cond_signal(pthread_cond_t *);
int pthread_condattr_init(pthread_condattr_t *);
int pthread_condattr_destroy(pthread_condattr_t *);
int pthread_condattr_getclock(const pthread_condattr_t *__restrict, clockid_t *__restrict);
int pthread_condattr_setclock(pthread_condattr_t *, clockid_t);
int pthread_condattr_getpshared(const pthread_condattr_t *__restrict, int *__restrict);
int pthread_condattr_setpshared(pthread_condattr_t *, int);

int pthread_rwlock_init(pthread_rwlock_t *__restrict, const pthread_rwlockattr_t *__restrict);
int pthread_rwlock_destroy(pthread_rwlock_t *);
int pthread_rwlock_rdlock(pthread_rwlock_t *);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *);
int pthread_rwlock_timedrdlock(pthread_rwlock_t *__restrict, const struct timespec *__restrict);
int pthread_rwlock_clockrdlock(pthread_rwlock_t *__restrict, clockid_t, const struct timespec *__restrict);
int pthread_rwlock_wrlock(pthread_rwlock_t *);
int pthread_rwlock_trywrlock(pthread_rwlock_t *);
int pthread_rwlock_timedwrlock(pthread_rwlock_t *__restrict, const struct timespec *__restrict);
int pthread_rwlock_clockwrlock(pthread_rwlock_t *__restrict, clockid_t, const struct timespec *__restrict);
int pthread_rwlock_unlock(pthread_rwlock_t *);
int pthread_rwlockattr_init(pthread_rwlockattr_t *);
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *);
int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *__restrict, int *__restrict);
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *, int);

int pthread_spin_init(pthread_spinlock_t *, int);
int pthread_spin_destroy(pthread_spinlock_t *);
int pthread_spin_lock(pthread_spinlock_t *);
int pthread_spin_trylock(pthread_spinlock_t *);
int pthread_spin_unlock(pthread_spinlock_t *);

int pthread_barrier_init(pthread_barrier_t *__restrict, const pthread_barrierattr_t *__restrict, unsigned);
int pthread_barrier_destroy(pthread_barrier_t *);
int pthread_barrier_wait(pthread_barrier_t *);
int pthread_barrierattr_init(pthread_barrierattr_t *);
int pthread_barrierattr_destroy(pthread_barrierattr_t *);
int pthread_barrierattr_getpshared(const pthread_barrierattr_t *__restrict, int *__restrict);
int pthread_barrierattr_setpshared(pthread_barrierattr_t *, int);

int pthread_key_create(pthread_key_t *, void (*)(void *));
int pthread_key_delete(pthread_key_t);
void *pthread_getspecific(pthread_key_t);
int pthread_setspecific(pthread_key_t, const void *);

int pthread_attr_init(pthread_attr_t *);
int pthread_attr_destroy(pthread_attr_t *);
int pthread_attr_getguardsize(const pthread_attr_t *__restrict, size_t *__restrict);
int pthread_attr_setguardsize(pthread_attr_t *, size_t);
int pthread_attr_getstacksize(const pthread_attr_t *__restrict, size_t *__restrict);
int pthread_attr_setstacksize(pthread_attr_t *, size_t);
int pthread_attr_getdetachstate(const pthread_attr_t *, int *);
int pthread_attr_setdetachstate(pthread_attr_t *, int);
int pthread_attr_getstack(const pthread_attr_t *__restrict, void **__restrict, size_t *__restrict);
int pthread_attr_setstack(pthread_attr_t *, void *, size_t);
int pthread_attr_getscope(const pthread_attr_t *__restrict, int *__restrict);
int pthread_attr_setscope(pthread_attr_t *, int);
int pthread_attr_getschedpolicy(const pthread_attr_t *__restrict, int *__restrict);
int pthread_attr_setschedpolicy(pthread_attr_t *, int);
int pthread_attr_getschedparam(const pthread_attr_t *__restrict, struct sched_param *__restrict);
int pthread_attr_setschedparam(pthread_attr_t *__restrict, const struct sched_param *__restrict);
int pthread_attr_getinheritsched(const pthread_attr_t *__restrict, int *__restrict);
int pthread_attr_setinheritsched(pthread_attr_t *, int);

/* Cleanup handlers: a frame on the caller's stack linked into the thread. */
struct __spfxd_cleanup {
	void (*__fn)(void *);
	void *__arg;
	struct __spfxd_cleanup *__next;
};
void __spfxd_cleanup_push(struct __spfxd_cleanup *, void (*)(void *), void *);
void __spfxd_cleanup_pop(struct __spfxd_cleanup *, int);
#define pthread_cleanup_push(f, a) do { struct __spfxd_cleanup __cb; \
	__spfxd_cleanup_push(&__cb, f, a);
#define pthread_cleanup_pop(r) __spfxd_cleanup_pop(&__cb, (r)); } while (0)

#if defined(__SPFXD_GNU)
int pthread_getattr_np(pthread_t, pthread_attr_t *);
int pthread_setname_np(pthread_t, const char *);
int pthread_getname_np(pthread_t, char *, size_t);
int pthread_tryjoin_np(pthread_t, void **);
int pthread_timedjoin_np(pthread_t, void **, const struct timespec *);
int pthread_setaffinity_np(pthread_t, size_t, const cpu_set_t *);
int pthread_getaffinity_np(pthread_t, size_t, cpu_set_t *);
int pthread_yield(void);
pid_t pthread_gettid_np(pthread_t);
#endif

__SPFXD_END_DECLS
#endif
