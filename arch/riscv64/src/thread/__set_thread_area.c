/* lib-spfxd — install the thread pointer (RISC-V: the tp register).
 * struct pthread sits just below tp (TLS variant I, no TCB gap). */
#include "pthread_impl.h"

hidden int __set_thread_area(void *p)
{
	__asm__ __volatile__ ("mv tp, %0" : : "r"(TP_ADJ(p)) : "memory");
	return 0;
}
