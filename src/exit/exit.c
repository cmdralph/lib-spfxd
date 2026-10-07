/*
 * lib-spfxd — exit / _Exit.
 *
 * Termination order: atexit/__cxa_atexit handlers (which include the
 * executable's destructors, registered at startup), shared-library
 * destructors, flushing of every stdio stream, then the exit_group system
 * call.  Subsystems that may not be linked into a static program are
 * reached through weak aliases of a no-op, so exit() never pulls in stdio
 * or the dynamic linker by itself.
 */
#include <stdlib.h>
#include <unistd.h>
#include "libc.h"
#include "pthread_impl.h"

static void dummy(void) { }
weak_alias(dummy, __libc_exit_fini);
weak_alias(dummy, __stdio_exit);

_Noreturn void _Exit(int code)
{
	for (;;) {
		__syscall(SYS_exit_group, code);
		__syscall(SYS_exit, code);
	}
}
weak_alias(_Exit, _exit);

static volatile int exit_owner;
hidden int __exit_status;        /* passed to on_exit handlers */

_Noreturn void exit(int code)
{
	/* Concurrent exit from another thread is undefined; serialize it by
	 * parking the late thread.  Re-entry from a handler is tolerated. */
	int tid = __self()->tid;
	int owner = a_cas(&exit_owner, 0, tid);
	if (owner && owner != tid)
		for (;;) __syscall(SYS_pause);

	__exit_status = code;
	__funcs_on_exit();
	__libc_exit_fini();
	__stdio_exit();
	_Exit(code);
}
