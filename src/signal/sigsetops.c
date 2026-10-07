/* lib-spfxd — signal set manipulation. */
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "pthread_impl.h"

#define WORD(s) (((unsigned)(s) - 1) / (8 * sizeof(long)))
#define BIT(s) (1UL << (((unsigned)(s) - 1) % (8 * sizeof(long))))

int sigemptyset(sigset_t *set)
{
	memset(set, 0, sizeof *set);
	return 0;
}

/* Only signals the kernel knows are set, and never the internal ones. */
int sigfillset(sigset_t *set)
{
	memset(set, 0, sizeof *set);
	set->__bits[0] = ~0UL;
	__sig_strip_internal(set);
	return 0;
}

static int valid(int s)
{
	return s > 0 && s < _NSIG;
}

int sigaddset(sigset_t *set, int s)
{
	if (!valid(s) || s == SIGCANCEL || s == SIGSYNCCALL) {
		errno = EINVAL;
		return -1;
	}
	set->__bits[WORD(s)] |= BIT(s);
	return 0;
}

int sigdelset(sigset_t *set, int s)
{
	if (!valid(s) || s == SIGCANCEL || s == SIGSYNCCALL) {
		errno = EINVAL;
		return -1;
	}
	set->__bits[WORD(s)] &= ~BIT(s);
	return 0;
}

int sigismember(const sigset_t *set, int s)
{
	if (!valid(s)) {
		errno = EINVAL;
		return -1;
	}
	return !!(set->__bits[WORD(s)] & BIT(s));
}

int sigisemptyset(const sigset_t *set)
{
	for (size_t i = 0; i < 16; i++)
		if (set->__bits[i]) return 0;
	return 1;
}

int sigorset(sigset_t *d, const sigset_t *a, const sigset_t *b)
{
	for (size_t i = 0; i < 16; i++) d->__bits[i] = a->__bits[i] | b->__bits[i];
	return 0;
}

int sigandset(sigset_t *d, const sigset_t *a, const sigset_t *b)
{
	for (size_t i = 0; i < 16; i++) d->__bits[i] = a->__bits[i] & b->__bits[i];
	return 0;
}

int __libc_current_sigrtmin(void) { return SIGRT_FIRST_USER; }
int __libc_current_sigrtmax(void) { return _NSIG - 1; }
