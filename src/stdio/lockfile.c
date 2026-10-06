/*
 * lib-spfxd — stream locking.
 *
 * The lock word holds the owner's thread id, with MAYBE_WAITERS set once
 * anyone has slept on it.  Internal operations lock through FLOCK, which
 * skips locking while the process is single-threaded and when the calling
 * thread already owns the stream via flockfile().  flockfile() always
 * locks, so explicit locking stays consistent across pthread_create.
 */
#include <limits.h>
#include "stdio_impl.h"
#include "pthread_impl.h"

static void lock_acquire(FILE *f, int tid)
{
	int want = tid, cur;
	while ((cur = a_cas(&f->lock, 0, want))) {
		if (!(cur & MAYBE_WAITERS) && a_cas(&f->lock, cur, cur | MAYBE_WAITERS) != cur)
			continue;
		__futex_wait(&f->lock, cur | MAYBE_WAITERS, 1);
		/* we may have been one of several sleepers: stay conservative */
		want = tid | MAYBE_WAITERS;
	}
}

hidden int __lockfile(FILE *f)
{
	if (f->flags & F_NOLOCK) return 0;
	int tid = __self()->tid;
	if ((f->lock & ~MAYBE_WAITERS) == tid) return 0;
	lock_acquire(f, tid);
	return 1;
}

hidden void __unlockfile(FILE *f)
{
	if (a_swap(&f->lock, 0) & MAYBE_WAITERS)
		__futex_wake(&f->lock, 1, 1);
}

int ftrylockfile(FILE *f)
{
	int tid = __self()->tid;
	if ((f->lock & ~MAYBE_WAITERS) == tid) {
		if (f->lockcount == INT_MAX) return -1;
		f->lockcount++;
		return 0;
	}
	if (a_cas(&f->lock, 0, tid)) return -1;
	f->lockcount = 1;
	return 0;
}

void flockfile(FILE *f)
{
	if (!ftrylockfile(f)) return;
	lock_acquire(f, __self()->tid);
	f->lockcount = 1;
}

void funlockfile(FILE *f)
{
	if (f->lockcount == 1) {
		f->lockcount = 0;
		__unlockfile(f);
	} else {
		f->lockcount--;
	}
}

/* After fork the child has a single thread with a new tid: streams the
 * forking thread held stay held (under the new tid), all others are free. */
static void fix_lock(FILE *f, int old_tid, int tid)
{
	f->lock = (f->lock & ~MAYBE_WAITERS) == old_tid ? tid : 0;
	if (!f->lock) f->lockcount = 0;
}

hidden void __stdio_atfork(int who)
{
	static int old_tid;
	static FILE **head;
	if (who < 0) {
		head = __ofl_lock();
		old_tid = __self()->tid;
		return;
	}
	if (who > 0) {
		int tid = __self()->tid;
		fix_lock(&__stdin_FILE, old_tid, tid);
		fix_lock(&__stdout_FILE, old_tid, tid);
		fix_lock(&__stderr_FILE, old_tid, tid);
		for (FILE *f = *head; f; f = f->next) fix_lock(f, old_tid, tid);
	}
	__ofl_unlock();
}
