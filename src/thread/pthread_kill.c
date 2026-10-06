/* lib-spfxd — pthread_kill and scheduling/affinity/naming helpers. */
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "pthread_impl.h"

int pthread_kill(pthread_t th, int sig)
{
	struct pthread *t = (struct pthread *)th;
	sigset_t set;
	int r;
	/* Holding the list lock keeps an exiting thread's tid from being
	 * reused while we signal it. */
	__block_app_sigs(&set);
	__tl_lock();
	r = t->tid ? (int)-__syscall(SYS_tkill, t->tid, sig) : (sig < 0 || sig >= _NSIG ? EINVAL : 0);
	__tl_unlock();
	__restore_sigs(&set);
	return r;
}

int pthread_getschedparam(pthread_t th, int *restrict policy, struct sched_param *restrict param)
{
	struct pthread *t = (struct pthread *)th;
	long r = __syscall(SYS_sched_getparam, t->tid, param);
	if (r) return (int)-r;
	r = __syscall(SYS_sched_getscheduler, t->tid);
	if (r < 0) return (int)-r;
	*policy = (int)r;
	return 0;
}

int pthread_setschedparam(pthread_t th, int policy, const struct sched_param *param)
{
	return (int)-__syscall(SYS_sched_setscheduler, ((struct pthread *)th)->tid, policy, param);
}

int pthread_setschedprio(pthread_t th, int prio)
{
	struct sched_param p;
	memset(&p, 0, sizeof p);
	p.sched_priority = prio;
	return (int)-__syscall(SYS_sched_setparam, ((struct pthread *)th)->tid, &p);
}

int pthread_getcpuclockid(pthread_t th, clockid_t *clk)
{
	/* Linux encoding of a per-thread CPU clock: (~tid << 3) | 6 */
	*clk = (clockid_t)((~(unsigned)((struct pthread *)th)->tid << 3) | 6);
	return 0;
}

int pthread_setaffinity_np(pthread_t th, size_t size, const cpu_set_t *set)
{
	return (int)-__syscall(SYS_sched_setaffinity, ((struct pthread *)th)->tid, size, set);
}

int pthread_getaffinity_np(pthread_t th, size_t size, cpu_set_t *set)
{
	long r = __syscall(SYS_sched_getaffinity, ((struct pthread *)th)->tid, size, set);
	if (r < 0) return (int)-r;
	if ((size_t)r < size) memset((char *)set + r, 0, size - (size_t)r);
	return 0;
}

static int comm_path(pthread_t th, char *buf, size_t n)
{
	return snprintf(buf, n, "/proc/self/task/%d/comm", ((struct pthread *)th)->tid);
}

int pthread_setname_np(pthread_t th, const char *name)
{
	size_t len = strnlen(name, 16);
	if (len > 15) return ERANGE;
	if ((struct pthread *)th == __self())
		return (int)-__syscall(SYS_prctl, 15 /* PR_SET_NAME */, name, 0, 0, 0);
	char path[48];
	comm_path(th, path, sizeof path);
	int fd = (int)__syscall(SYS_open, path, O_WRONLY | O_CLOEXEC);
	if (fd < 0) return -fd;
	long r = __syscall(SYS_write, fd, name, len);
	__syscall(SYS_close, fd);
	return r < 0 ? (int)-r : 0;
}

int pthread_getname_np(pthread_t th, char *name, size_t len)
{
	if (len < 16) return ERANGE;
	if ((struct pthread *)th == __self())
		return (int)-__syscall(SYS_prctl, 16 /* PR_GET_NAME */, name, 0, 0, 0);
	char path[48];
	comm_path(th, path, sizeof path);
	int fd = (int)__syscall(SYS_open, path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) return -fd;
	long r = __syscall(SYS_read, fd, name, len - 1);
	__syscall(SYS_close, fd);
	if (r < 0) return (int)-r;
	if (r > 0 && name[r - 1] == '\n') r--;
	name[r] = 0;
	return 0;
}

int pthread_getconcurrency(void) { return 0; }
int pthread_setconcurrency(int n) { return n < 0 ? EINVAL : 0; }
int pthread_yield(void) { return (int)-__syscall(SYS_sched_yield); }
