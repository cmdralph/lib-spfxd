/*
 * lib-spfxd — thread control block (TCB) and threading internals.
 *
 * Every thread, including the initial one, owns a struct pthread that the
 * %fs base points to.  The first words have fixed offsets demanded by the
 * x86-64 ABI and by compiler-generated code:
 *
 *   0x00 self    TLS ABI: `mov %fs:0,%reg` yields the thread pointer
 *   0x08 dtv     dynamic thread vector for __tls_get_addr
 *   0x28 canary  -fstack-protector reads %fs:0x28
 *
 * Memory layout of one thread's allocation (TLS variant II):
 *
 *   low  [guard page(s)][stack ... grows down][dtv][TLS blocks][struct pthread] high
 *                                                              ^ thread pointer
 */
#ifndef _SPFXD_PTHREAD_IMPL_H
#define _SPFXD_PTHREAD_IMPL_H

#include <pthread.h>
#include <signal.h>
#include <limits.h>
#include "libc.h"
#include "atomic.h"
#include "lock.h"
#include "syscall.h"

struct malloc_tcache;

enum {
	DT_EXITED = 0,      /* thread has finished (joinable threads only) */
	DT_EXITING,         /* thread is running its exit path */
	DT_JOINABLE,
	DT_DETACHED,
};

struct pthread {
	/* ---- ABI-fixed prefix: do not reorder ---- */
	struct pthread *self;
	uintptr_t *dtv;
	struct pthread *prev, *next;      /* circular list of live threads */
	uintptr_t sysinfo;
	uintptr_t canary;
	uintptr_t canary2;
	/* ---- library-private ---- */
	volatile int tid;                 /* kernel thread id; cleared by the kernel at exit */
	int errno_val;
	volatile int detach_state;
	volatile int cancel;              /* cancellation requested */
	volatile unsigned char cancel_disable, cancel_async;
	unsigned char tsd_used;
	unsigned char is_main;
	void *(*start)(void *);
	void *start_arg;
	void *result;
	unsigned char *map_base;          /* mapping to release when the thread is reaped */
	size_t map_size;
	void *stack;                      /* highest usable stack address */
	size_t stack_size;
	size_t guard_size;
	struct __spfxd_cleanup *cleanup;
	struct __spfxd_locale *locale;
	struct malloc_tcache *tcache;
	volatile int killlock;            /* held while tid may be targeted by tgkill */
	char *dlerror_buf;
	int dlerror_flag;
	sigset_t sigmask;                 /* signal mask the new thread starts with */
	void *tsd[PTHREAD_KEYS_MAX];
	char name[16];
	char strerror_buf[48];
};

static __inline struct pthread *__self(void)
{
	return (struct pthread *)__arch_tp();
}

/* Internal signals: never deliverable to applications, never blockable by
 * them (sigprocmask and friends strip them). */
#define SIGCANCEL 32
#define SIGSYNCCALL 33
#define SIGRT_FIRST_USER 34

/* Process-wide list of threads, protected by __thread_list_lock. */
extern hidden volatile int __thread_list_lock;
extern hidden volatile int __thread_count;

hidden void __tl_lock(void);
hidden void __tl_unlock(void);
hidden void __tl_sync(struct pthread *);

hidden int __clone(int (*)(void *), void *, int, void *, ...);
hidden int __set_thread_area(void *);
hidden void __unmapself(void *, size_t) __attribute__((__noreturn__));
hidden void __pthread_tsd_run_dtors(void);
hidden void __do_cleanup_push(struct __spfxd_cleanup *);
hidden void __do_cleanup_pop(struct __spfxd_cleanup *);
hidden __attribute__((__noreturn__)) void __pthread_exit_internal(void *);
hidden void __testcancel(void);
hidden long __cancel(void);
hidden long __syscall_cp_asm(volatile int *, long, long, long, long, long, long, long);
hidden void __synccall(void (*)(void *), void *);
hidden void __block_all_sigs(void *);
hidden void __block_app_sigs(void *);
hidden void __restore_sigs(void *);
hidden void __sig_strip_internal(sigset_t *);
hidden int __pthread_mutex_lock_internal(pthread_mutex_t *);
hidden int __pthread_mutex_unlock_internal(pthread_mutex_t *);
hidden int __pthread_mutex_timedlock_internal(pthread_mutex_t *, clockid_t, const struct timespec *);
hidden int __pthread_cond_timedwait_internal(pthread_cond_t *, pthread_mutex_t *, clockid_t, const struct timespec *);
hidden void __malloc_thread_exit(struct pthread *);
hidden void __fork_handler(int);
hidden void __malloc_atfork(int);
hidden void __stdio_atfork(int);
hidden void __ldso_atfork(int);
hidden void __tls_thread_exit(struct pthread *);

#define DEFAULT_STACK_SIZE (2 << 20)
#define DEFAULT_GUARD_SIZE 4096
#define DEFAULT_STACK_MAX (64 << 20)

#endif

_Static_assert(__builtin_offsetof(struct pthread, self) == 0, "TCB self offset");
_Static_assert(__builtin_offsetof(struct pthread, dtv) == 8, "TCB dtv offset");
_Static_assert(__builtin_offsetof(struct pthread, canary) == TCB_CANARY_OFFSET, "TCB canary offset");
_Static_assert(__builtin_offsetof(struct pthread, errno_val) == 0x3c, "TCB errno offset (see internal errno.h)");
