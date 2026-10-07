/* lib-spfxd — sysconf / pathconf / fpathconf / confstr. */
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <semaphore.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/statfs.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include "libc.h"

long sysconf(int name)
{
	struct rlimit r;
	switch (name) {
	case _SC_ARG_MAX:
		if (!getrlimit(RLIMIT_STACK, &r) && r.rlim_cur != RLIM_INFINITY && r.rlim_cur / 4 > ARG_MAX)
			return (long)(r.rlim_cur / 4);
		return ARG_MAX;
	case _SC_CHILD_MAX:
		if (getrlimit(RLIMIT_NPROC, &r)) return -1;
		return r.rlim_cur == RLIM_INFINITY ? -1 : (long)r.rlim_cur;
	case _SC_CLK_TCK: return 100;
	case _SC_NGROUPS_MAX: return NGROUPS_MAX;
	case _SC_OPEN_MAX:
		if (getrlimit(RLIMIT_NOFILE, &r)) return -1;
		return r.rlim_cur == RLIM_INFINITY ? -1 : (long)r.rlim_cur;
	case _SC_STREAM_MAX: return FOPEN_MAX;
	case _SC_TZNAME_MAX: return TZNAME_MAX;
	case _SC_JOB_CONTROL: case _SC_SAVED_IDS: return 1;
	case _SC_REALTIME_SIGNALS: case _SC_VERSION: case _SC_THREADS:
	case _SC_THREAD_SAFE_FUNCTIONS: case _SC_MONOTONIC_CLOCK:
		return _POSIX_VERSION;
	case _SC_2_VERSION: return _POSIX2_VERSION;
	case _SC_XOPEN_VERSION: return _XOPEN_VERSION;
	case _SC_PAGESIZE: return (long)(__libc.page_size ? __libc.page_size : 4096);
	case _SC_RTSIG_MAX: return _NSIG - 1 - SIGRTMIN;
	case _SC_SEM_NSEMS_MAX: return -1;
	case _SC_SEM_VALUE_MAX: return SEM_VALUE_MAX;
	case _SC_SIGQUEUE_MAX:
		if (getrlimit(RLIMIT_SIGPENDING, &r)) return -1;
		return (long)r.rlim_cur;
	case _SC_TIMER_MAX: return -1;
	case _SC_BC_BASE_MAX: return BC_BASE_MAX;
	case _SC_BC_DIM_MAX: return BC_DIM_MAX;
	case _SC_BC_SCALE_MAX: return BC_SCALE_MAX;
	case _SC_BC_STRING_MAX: return BC_STRING_MAX;
	case _SC_COLL_WEIGHTS_MAX: return COLL_WEIGHTS_MAX;
	case _SC_EXPR_NEST_MAX: return EXPR_NEST_MAX;
	case _SC_LINE_MAX: return LINE_MAX;
	case _SC_RE_DUP_MAX: return RE_DUP_MAX;
	case _SC_IOV_MAX: return IOV_MAX;
	case _SC_GETGR_R_SIZE_MAX: case _SC_GETPW_R_SIZE_MAX: return -1;
	case _SC_LOGIN_NAME_MAX: return LOGIN_NAME_MAX;
	case _SC_TTY_NAME_MAX: return TTY_NAME_MAX;
	case _SC_THREAD_DESTRUCTOR_ITERATIONS: return PTHREAD_DESTRUCTOR_ITERATIONS;
	case _SC_THREAD_KEYS_MAX: return PTHREAD_KEYS_MAX;
	case _SC_THREAD_STACK_MIN: return PTHREAD_STACK_MIN;
	case _SC_THREAD_THREADS_MAX: return -1;
	case _SC_NPROCESSORS_CONF: return get_nprocs_conf();
	case _SC_NPROCESSORS_ONLN: return get_nprocs();
	case _SC_PHYS_PAGES: return get_phys_pages();
	case _SC_AVPHYS_PAGES: return get_avphys_pages();
	case _SC_ATEXIT_MAX: return -1;
	case _SC_PASS_MAX: return 8192;
	case _SC_HOST_NAME_MAX: return HOST_NAME_MAX;
	case _SC_SYMLOOP_MAX: return SYMLOOP_MAX;
	case _SC_LEVEL1_DCACHE_LINESIZE: return 64;
	}
	errno = EINVAL;
	return -1;
}

static long path_limit(int name, long fs_namelen)
{
	switch (name) {
	case _PC_LINK_MAX: return LINK_MAX;
	case _PC_MAX_CANON: return MAX_CANON;
	case _PC_MAX_INPUT: return MAX_INPUT;
	case _PC_NAME_MAX: return fs_namelen > 0 ? fs_namelen : NAME_MAX;
	case _PC_PATH_MAX: return PATH_MAX;
	case _PC_PIPE_BUF: return PIPE_BUF;
	case _PC_CHOWN_RESTRICTED: return 1;
	case _PC_NO_TRUNC: return 1;
	case _PC_VDISABLE: return 0;
	case _PC_SYNC_IO: return 1;
	case _PC_ASYNC_IO: case _PC_PRIO_IO: return -1;
	case _PC_FILESIZEBITS: return FILESIZEBITS;
	case _PC_SYMLINK_MAX: return -1;
	}
	errno = EINVAL;
	return -1;
}

long fpathconf(int fd, int name)
{
	struct statfs st;
	if (fstatfs(fd, &st)) return -1;
	return path_limit(name, st.f_namelen);
}

long pathconf(const char *path, int name)
{
	struct statfs st;
	if (statfs(path, &st)) return -1;
	return path_limit(name, st.f_namelen);
}

size_t confstr(int name, char *buf, size_t len)
{
	const char *s;
	if (name == _CS_PATH) s = "/bin:/usr/bin";
	else {
		errno = EINVAL;
		return 0;
	}
	size_t l = strlen(s) + 1;
	if (buf && len) {
		size_t k = l < len ? l : len;
		memcpy(buf, s, k - 1);
		buf[k - 1] = 0;
	}
	return l;
}
