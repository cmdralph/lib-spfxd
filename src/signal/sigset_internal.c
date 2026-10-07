/*
 * lib-spfxd — signal mask helpers used inside the library.
 *
 * The kernel's signal mask is 64 bits; sigset_t is larger for compatibility,
 * so only the first word is ever passed (sigsetsize = 8).
 */
#include <signal.h>
#include "pthread_impl.h"

#define KSIGSET 8

static const unsigned long all_mask = ~0UL;
static const unsigned long app_mask = ~0UL & ~(3UL << (SIGCANCEL - 1));

hidden void __block_all_sigs(void *set)
{
	__syscall(SYS_rt_sigprocmask, SIG_BLOCK, &all_mask, set, KSIGSET);
}

hidden void __block_app_sigs(void *set)
{
	__syscall(SYS_rt_sigprocmask, SIG_BLOCK, &app_mask, set, KSIGSET);
}

hidden void __restore_sigs(void *set)
{
	__syscall(SYS_rt_sigprocmask, SIG_SETMASK, set, 0, KSIGSET);
}

/* Applications may never block or catch the library's internal signals. */
hidden void __sig_strip_internal(sigset_t *set)
{
	set->__bits[0] &= ~(3UL << (SIGCANCEL - 1));
}
