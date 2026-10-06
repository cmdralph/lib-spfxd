/*
 * lib-spfxd — POSIX per-process timers.
 *
 * SIGEV_SIGNAL, SIGEV_NONE and SIGEV_THREAD_ID map directly onto the kernel
 * timer; timer_t holds the kernel timer id.
 *
 * SIGEV_THREAD gets a dedicated helper thread with every signal blocked.
 * The kernel timer targets that thread (SIGEV_THREAD_ID) with SIGCANCEL,
 * and the helper collects expirations synchronously with sigtimedwait,
 * calling the notification function for each.  Such timers are encoded as
 * the bitwise complement of the helper's thread pointer, which is always a
 * negative value and so cannot collide with a kernel id.
 */
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pthread_impl.h"

struct ksigevent {
	union sigval value;
	int signo;
	int notify;
	int tid;
	int pad[11];
};

struct helper {
	void (*fn)(union sigval);
	union sigval val;
	int ktimer;
	volatile int ready;
	volatile int stop;
	pthread_t th;
};

static void *helper_main(void *p)
{
	struct helper *h = p;
	unsigned long set = 1UL << (SIGCANCEL - 1);
	siginfo_t si;

	a_store(&h->ready, 1);
	__futex_wake(&h->ready, 1, 1);
	for (;;) {
		long r = __syscall(SYS_rt_sigtimedwait, &set, &si, 0, 8);
		if (r < 0) continue;
		if (a_load(&h->stop)) break;
		if (si.si_code == SI_TIMER) h->fn(h->val);
	}
	__syscall(SYS_timer_delete, h->ktimer);
	free(h);
	return 0;
}

int timer_create(clockid_t clk, struct sigevent *restrict sev, timer_t *restrict res)
{
	struct ksigevent ksev;
	int id;

	if (!sev || sev->sigev_notify != SIGEV_THREAD) {
		memset(&ksev, 0, sizeof ksev);
		if (sev) {
			ksev.value = sev->sigev_value;
			ksev.signo = sev->sigev_signo;
			ksev.notify = sev->sigev_notify;
			ksev.tid = sev->sigev_notify_thread_id;
		}
		if (__syscall_ret((unsigned long)__syscall(SYS_timer_create, clk, sev ? &ksev : 0, &id)) < 0)
			return -1;
		*res = (timer_t)(intptr_t)id;
		return 0;
	}

	struct helper *h = calloc(1, sizeof *h);
	if (!h) return -1;
	h->fn = sev->sigev_notify_function;
	h->val = sev->sigev_value;

	pthread_attr_t attr;
	if (sev->sigev_notify_attributes) attr = *sev->sigev_notify_attributes;
	else pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	sigset_t old;
	__block_all_sigs(&old);   /* the helper inherits a fully blocked mask */
	int e = pthread_create(&h->th, &attr, helper_main, h);
	if (e) {
		__restore_sigs(&old);
		free(h);
		errno = EAGAIN;
		return -1;
	}
	__restore_sigs(&old);
	while (!a_load(&h->ready)) __futex_wait(&h->ready, 0, 1);

	struct pthread *td = (struct pthread *)h->th;
	memset(&ksev, 0, sizeof ksev);
	ksev.value.sival_ptr = h;
	ksev.signo = SIGCANCEL;
	ksev.notify = SIGEV_THREAD_ID;
	ksev.tid = td->tid;
	if (__syscall_ret((unsigned long)__syscall(SYS_timer_create, clk, &ksev, &id)) < 0) {
		a_store(&h->stop, 1);
		__syscall(SYS_tkill, td->tid, SIGCANCEL);
		return -1;
	}
	h->ktimer = id;
	*res = (timer_t)~(uintptr_t)h;
	return 0;
}

static int kernel_id(timer_t t, struct helper **h)
{
	intptr_t v = (intptr_t)t;
	if (v < 0) {
		*h = (struct helper *)~(uintptr_t)v;
		return (*h)->ktimer;
	}
	*h = 0;
	return (int)v;
}

int timer_delete(timer_t t)
{
	struct helper *h;
	int id = kernel_id(t, &h);
	if (!h) return (int)__sysret(SYS_timer_delete, id);
	/* disarm, then tell the helper to clean up and exit */
	struct itimerspec zero = { { 0, 0 }, { 0, 0 } };
	__syscall(SYS_timer_settime, id, 0, &zero, 0);
	/* the helper frees h once it sees stop: read everything first */
	int tid = ((struct pthread *)h->th)->tid;
	a_store(&h->stop, 1);
	__syscall(SYS_tkill, tid, SIGCANCEL);
	return 0;
}

int timer_settime(timer_t t, int flags, const struct itimerspec *restrict v, struct itimerspec *restrict old)
{
	struct helper *h;
	return (int)__sysret(SYS_timer_settime, kernel_id(t, &h), flags, v, old);
}

int timer_gettime(timer_t t, struct itimerspec *v)
{
	struct helper *h;
	return (int)__sysret(SYS_timer_gettime, kernel_id(t, &h), v);
}

int timer_getoverrun(timer_t t)
{
	struct helper *h;
	return (int)__sysret(SYS_timer_getoverrun, kernel_id(t, &h));
}
