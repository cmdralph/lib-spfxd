/* lib-spfxd — wait / waitpid / wait3 / wait4 / waitid. */
#include <sys/wait.h>
#include <sys/resource.h>
#include <string.h>
#include "syscall.h"

pid_t wait4(pid_t pid, int *status, int options, struct rusage *ru)
{
	return (pid_t)__sysret_cp(SYS_wait4, pid, status, options, ru);
}

pid_t waitpid(pid_t pid, int *status, int options)
{
	return wait4(pid, status, options, 0);
}

pid_t wait(int *status)
{
	return wait4(-1, status, 0, 0);
}

pid_t wait3(int *status, int options, struct rusage *ru)
{
	return wait4(-1, status, options, ru);
}

int waitid(idtype_t type, id_t id, siginfo_t *info, int options)
{
	if (info) memset(info, 0, sizeof *info);
	return (int)__sysret_cp(SYS_waitid, type, id, info, options, 0);
}
