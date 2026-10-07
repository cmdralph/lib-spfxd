/* lib-spfxd — system identification and resource interfaces. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "syscall.h"

int uname(struct utsname *u) { return (int)__sysret(SYS_uname, u); }

int gethostname(char *name, size_t len)
{
	struct utsname u;
	if (uname(&u)) return -1;
	size_t i;
	for (i = 0; i < len && (name[i] = u.nodename[i]); i++);
	if (i == len) {
		if (len) name[len - 1] = 0;
		errno = ENAMETOOLONG;
		return -1;
	}
	return 0;
}

int sethostname(const char *name, size_t len) { return (int)__sysret(SYS_sethostname, name, len); }
int setdomainname(const char *name, size_t len) { return (int)__sysret(SYS_setdomainname, name, len); }

int getdomainname(char *name, size_t len)
{
	struct utsname u;
	if (uname(&u)) return -1;
	size_t l = strlen(u.domainname);
	if (l >= len) {
		errno = EINVAL;
		return -1;
	}
	memcpy(name, u.domainname, l + 1);
	return 0;
}

long gethostid(void) { return 0; }

int getrlimit(int res, struct rlimit *r) { return (int)__sysret(SYS_prlimit64, 0, res, 0, r); }
int setrlimit(int res, const struct rlimit *r) { return (int)__sysret(SYS_prlimit64, 0, res, r, 0); }
int prlimit(pid_t pid, int res, const struct rlimit *n, struct rlimit *o)
{
	return (int)__sysret(SYS_prlimit64, pid, res, n, o);
}
weak_alias(getrlimit, getrlimit64);
weak_alias(setrlimit, setrlimit64);

int getrusage(int who, struct rusage *ru) { return (int)__sysret(SYS_getrusage, who, ru); }

/* The kernel returns 20 - nice so the result is never negative. */
int getpriority(int which, id_t who)
{
	long r = __syscall(SYS_getpriority, which, who);
	if (r < 0) return (int)__syscall_ret((unsigned long)r);
	return 20 - (int)r;
}

int setpriority(int which, id_t who, int prio) { return (int)__sysret(SYS_setpriority, which, who, prio); }

int nice(int inc)
{
	int prio = inc;
	if (inc > -2 * NZERO && inc < 2 * NZERO) prio += getpriority(PRIO_PROCESS, 0);
	if (prio > NZERO - 1) prio = NZERO - 1;
	if (prio < -NZERO) prio = -NZERO;
	if (setpriority(PRIO_PROCESS, 0, prio)) {
		if (errno == EACCES) errno = EPERM;
		return -1;
	}
	return prio;
}

clock_t times(struct tms *t) { return (clock_t)__syscall(SYS_times, t); }

int sysinfo(struct sysinfo *info) { return (int)__sysret(SYS_sysinfo, info); }

int get_nprocs_conf(void)
{
	/* count possible CPUs from /sys; fall back to the online count */
	FILE *f = fopen("/sys/devices/system/cpu/possible", "re");
	if (f) {
		int lo, hi, n = 0, c;
		while (fscanf(f, "%d", &lo) == 1) {
			hi = lo;
			if ((c = fgetc(f)) == '-') {
				if (fscanf(f, "%d", &hi) != 1) break;
				c = fgetc(f);
			}
			n += hi - lo + 1;
			if (c != ',') break;
		}
		fclose(f);
		if (n > 0) return n;
	}
	return get_nprocs();
}

int get_nprocs(void)
{
	unsigned long set[128];
	long r = __syscall(SYS_sched_getaffinity, 0, sizeof set, set);
	if (r <= 0) return 1;
	int n = 0;
	for (long i = 0; i < r / (long)sizeof(long); i++) n += __builtin_popcountl(set[i]);
	return n ? n : 1;
}

long get_phys_pages(void)
{
	struct sysinfo si;
	if (sysinfo(&si)) return -1;
	return (long)(si.totalram * si.mem_unit / 4096);
}

long get_avphys_pages(void)
{
	struct sysinfo si;
	if (sysinfo(&si)) return -1;
	return (long)((si.freeram + si.bufferram) * si.mem_unit / 4096);
}

int getloadavg(double *a, int n)
{
	struct sysinfo si;
	if (n <= 0) return n ? -1 : 0;
	if (sysinfo(&si)) return -1;
	if (n > 3) n = 3;
	for (int i = 0; i < n; i++) a[i] = si.loads[i] / 65536.0;
	return n;
}

int getpagesize(void) { return 4096; }

int getdtablesize(void)
{
	struct rlimit r;
	getrlimit(RLIMIT_NOFILE, &r);
	return r.rlim_cur < INT_MAX ? (int)r.rlim_cur : INT_MAX;
}

/* sched */
int sched_yield(void) { return (int)__sysret(SYS_sched_yield); }
int sched_get_priority_max(int p) { return (int)__sysret(SYS_sched_get_priority_max, p); }
int sched_get_priority_min(int p) { return (int)__sysret(SYS_sched_get_priority_min, p); }
int sched_getparam(pid_t pid, struct sched_param *p) { return (int)__sysret(SYS_sched_getparam, pid, p); }
int sched_setparam(pid_t pid, const struct sched_param *p) { return (int)__sysret(SYS_sched_setparam, pid, p); }
int sched_getscheduler(pid_t pid) { return (int)__sysret(SYS_sched_getscheduler, pid); }
int sched_setscheduler(pid_t pid, int pol, const struct sched_param *p) { return (int)__sysret(SYS_sched_setscheduler, pid, pol, p); }
int sched_rr_get_interval(pid_t pid, struct timespec *ts) { return (int)__sysret(SYS_sched_rr_get_interval, pid, ts); }
int sched_setaffinity(pid_t pid, size_t size, const cpu_set_t *set) { return (int)__sysret(SYS_sched_setaffinity, pid, size, set); }

int sched_getaffinity(pid_t pid, size_t size, cpu_set_t *set)
{
	long r = __syscall(SYS_sched_getaffinity, pid, size, set);
	if (r < 0) return (int)__syscall_ret((unsigned long)r);
	if ((size_t)r < size) memset((char *)set + r, 0, size - (size_t)r);
	return 0;
}

int __sched_cpucount(size_t size, const cpu_set_t *set)
{
	int n = 0;
	const unsigned char *p = (const unsigned char *)set;
	for (size_t i = 0; i < size; i++) n += __builtin_popcount(p[i]);
	return n;
}

int sched_getcpu(void)
{
	unsigned cpu;
	long r = __syscall(SYS_getcpu, &cpu, 0, 0);
	if (r < 0) return (int)__syscall_ret((unsigned long)r);
	return (int)cpu;
}

int unshare(int flags) { return (int)__sysret(SYS_unshare, flags); }
int setns(int fd, int type) { return (int)__sysret(SYS_setns, fd, type); }
