/* lib-spfxd — <sys/wait.h> */
#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H
#include <features.h>

#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_id_t
#include <bits/typedefs.h>
#include <signal.h>
#include <bits/siginfo.h>

typedef enum { P_ALL = 0, P_PID = 1, P_PGID = 2, P_PIDFD = 3 } idtype_t;

#define WNOHANG    1
#define WUNTRACED  2
#define WSTOPPED   2
#define WEXITED    4
#define WCONTINUED 8
#define WNOWAIT    0x01000000
#define __WNOTHREAD 0x20000000
#define __WALL      0x40000000
#define __WCLONE    0x80000000

#define WEXITSTATUS(s) (((s) & 0xff00) >> 8)
#define WTERMSIG(s)    ((s) & 0x7f)
#define WSTOPSIG(s)    WEXITSTATUS(s)
#define WCOREDUMP(s)   ((s) & 0x80)
#define WIFEXITED(s)   (!WTERMSIG(s))
/* Status word: low 7 bits = terminating signal (0 = exited, 0x7f = stopped),
 * bit 7 = core dump, bits 8..15 = exit code or stop signal; 0xffff = continued. */
static __inline int __spfxd_wifstopped(int __s)
{ return (__s & 0xff) == 0x7f && (__s & 0xffff) != 0xffff; }
static __inline int __spfxd_wifsignaled(int __s)
{ return (__s & 0x7f) != 0 && (__s & 0x7f) != 0x7f; }
#define WIFSTOPPED(s)  __spfxd_wifstopped(s)
#define WIFSIGNALED(s) __spfxd_wifsignaled(s)
#define WIFCONTINUED(s) ((s) == 0xffff)
#define W_EXITCODE(ret, sig) ((ret) << 8 | (sig))
#define W_STOPCODE(sig) ((sig) << 8 | 0x7f)

__SPFXD_BEGIN_DECLS
struct rusage;
pid_t wait(int *);
pid_t waitpid(pid_t, int *, int);
int waitid(idtype_t, id_t, siginfo_t *, int);
#if defined(__SPFXD_BSD)
pid_t wait3(int *, int, struct rusage *);
pid_t wait4(pid_t, int *, int, struct rusage *);
#endif
__SPFXD_END_DECLS
#endif
