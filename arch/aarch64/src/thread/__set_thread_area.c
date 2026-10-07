/* lib-spfxd — install the thread pointer (AArch64: TPIDR_EL0).  struct
 * pthread sits just below the thread pointer (TLS variant I). */
#include "pthread_impl.h"

hidden int __set_thread_area(void *p)
{
	__asm__ __volatile__ ("msr tpidr_el0, %0" : : "r"(TP_ADJ(p)) : "memory");
	return 0;
}
