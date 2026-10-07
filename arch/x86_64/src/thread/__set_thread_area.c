/* lib-spfxd — install the thread pointer (x86-64: %fs base via arch_prctl). */
#include "pthread_impl.h"

hidden int __set_thread_area(void *p)
{
	return (int)__syscall(SYS_arch_prctl, ARCH_SET_FS, p);
}
