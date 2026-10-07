/* lib-spfxd — <sys/prctl.h> */
#ifndef _SYS_PRCTL_H
#define _SYS_PRCTL_H
#include <features.h>
#define PR_SET_PDEATHSIG 1
#define PR_GET_PDEATHSIG 2
#define PR_GET_DUMPABLE 3
#define PR_SET_DUMPABLE 4
#define PR_SET_NAME 15
#define PR_GET_NAME 16
#define PR_SET_SECCOMP 22
#define PR_GET_SECCOMP 21
#define PR_SET_TIMERSLACK 29
#define PR_GET_TIMERSLACK 30
#define PR_SET_CHILD_SUBREAPER 36
#define PR_GET_CHILD_SUBREAPER 37
#define PR_SET_NO_NEW_PRIVS 38
#define PR_GET_NO_NEW_PRIVS 39
#define PR_SET_VMA 0x53564d41
#define PR_SET_VMA_ANON_NAME 0
__SPFXD_BEGIN_DECLS
int prctl(int, ...);
__SPFXD_END_DECLS
#endif
