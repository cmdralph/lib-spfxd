/* lib-spfxd — siglongjmp: restore the saved signal mask, then longjmp. */
#include <setjmp.h>
#include <signal.h>
#include "syscall.h"

_Noreturn void siglongjmp(sigjmp_buf env, int val)
{
	if (env->__fl) __syscall(SYS_rt_sigprocmask, SIG_SETMASK, env->__ss, 0, 8);
	longjmp(env, val);
}
