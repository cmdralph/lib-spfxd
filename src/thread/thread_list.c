/* lib-spfxd — the process-wide list of live threads. */
#include "pthread_impl.h"

hidden volatile int __thread_list_lock;
hidden volatile int __thread_count = 1;

hidden void __tl_lock(void)
{
	__lock_always(&__thread_list_lock);
}

hidden void __tl_unlock(void)
{
	__unlock_always(&__thread_list_lock);
}
