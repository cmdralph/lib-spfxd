/* lib-spfxd — process and credential identity queries. */
#include <unistd.h>
#include <errno.h>
#include "syscall.h"

pid_t getpid(void) { return (pid_t)__syscall(SYS_getpid); }
pid_t getppid(void) { return (pid_t)__syscall(SYS_getppid); }
uid_t getuid(void) { return (uid_t)__syscall(SYS_getuid); }
uid_t geteuid(void) { return (uid_t)__syscall(SYS_geteuid); }
gid_t getgid(void) { return (gid_t)__syscall(SYS_getgid); }
gid_t getegid(void) { return (gid_t)__syscall(SYS_getegid); }
pid_t getpgrp(void) { return (pid_t)__syscall(SYS_getpgid, 0); }
pid_t getpgid(pid_t pid) { return (pid_t)__sysret(SYS_getpgid, pid); }
pid_t getsid(pid_t pid) { return (pid_t)__sysret(SYS_getsid, pid); }
int setpgid(pid_t pid, pid_t pgid) { return (int)__sysret(SYS_setpgid, pid, pgid); }
pid_t setpgrp(void) { return setpgid(0, 0); }
pid_t setsid(void) { return (pid_t)__sysret(SYS_setsid); }

int getgroups(int n, gid_t list[])
{
	return (int)__sysret(SYS_getgroups, n, list);
}

int getresuid(uid_t *r, uid_t *e, uid_t *s) { return (int)__sysret(SYS_getresuid, r, e, s); }
int getresgid(gid_t *r, gid_t *e, gid_t *s) { return (int)__sysret(SYS_getresgid, r, e, s); }

int issetugid(void)
{
	return __libc.secure;
}
