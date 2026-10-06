/*
 * lib-spfxd — clocks and sleeping.
 *
 * clock_gettime goes through the vDSO when the kernel provides it (no
 * system call); the function pointer is resolved once, on first use.
 */
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "pthread_impl.h"

typedef int (*cgt_fn)(clockid_t, struct timespec *);

static int cgt_syscall(clockid_t clk, struct timespec *ts)
{
	return (int)__syscall(SYS_clock_gettime, clk, ts);
}

static int cgt_init(clockid_t clk, struct timespec *ts);
static void *volatile cgt_ptr = (void *)cgt_init;

static int cgt_init(clockid_t clk, struct timespec *ts)
{
	void *f = __vdsosym(VDSO_VER, VDSO_CGT_SYM);
	a_cas_p(&cgt_ptr, (void *)cgt_init, f ? f : (void *)cgt_syscall);
	return ((cgt_fn)cgt_ptr)(clk, ts);
}

int clock_gettime(clockid_t clk, struct timespec *ts)
{
	int r = ((cgt_fn)cgt_ptr)(clk, ts);
	if (!r) return 0;
	if (r == -ENOSYS || r == -EINVAL) {
		/* the vDSO declines some clocks; ask the kernel directly */
		r = cgt_syscall(clk, ts);
		if (!r) return 0;
	}
	errno = -r;
	return -1;
}

int clock_getres(clockid_t clk, struct timespec *ts)
{
	return (int)__sysret(SYS_clock_getres, clk, ts);
}

int clock_settime(clockid_t clk, const struct timespec *ts)
{
	return (int)__sysret(SYS_clock_settime, clk, ts);
}

int clock_nanosleep(clockid_t clk, int flags, const struct timespec *req, struct timespec *rem)
{
	if (clk == CLOCK_THREAD_CPUTIME_ID) return EINVAL;
	return (int)-__syscall_cp(SYS_clock_nanosleep, clk, flags, req, rem);
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
	return (int)__syscall_ret((unsigned long)-clock_nanosleep(CLOCK_REALTIME, 0, req, rem));
}

unsigned sleep(unsigned sec)
{
	struct timespec ts = { (time_t)sec, 0 };
	if (nanosleep(&ts, &ts)) return (unsigned)ts.tv_sec + (ts.tv_nsec > 0);
	return 0;
}

int usleep(useconds_t us)
{
	struct timespec ts = { (time_t)(us / 1000000), (long)(us % 1000000) * 1000 };
	return nanosleep(&ts, &ts);
}

time_t time(time_t *t)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	if (t) *t = ts.tv_sec;
	return ts.tv_sec;
}

int gettimeofday(struct timeval *restrict tv, void *restrict tz)
{
	struct timespec ts;
	if (tz) {
		struct timezone *z = tz;
		z->tz_minuteswest = 0;
		z->tz_dsttime = 0;
	}
	if (!tv) return 0;
	clock_gettime(CLOCK_REALTIME, &ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
	return 0;
}

int settimeofday(const struct timeval *tv, const struct timezone *tz)
{
	if (!tv) return 0;
	if ((unsigned long)tv->tv_usec >= 1000000UL) {
		errno = EINVAL;
		return -1;
	}
	struct timespec ts = { tv->tv_sec, tv->tv_usec * 1000 };
	return clock_settime(CLOCK_REALTIME, &ts);
}

int stime(const time_t *t)
{
	struct timeval tv = { *t, 0 };
	return settimeofday(&tv, 0);
}

int adjtime(const struct timeval *in, struct timeval *out)
{
	/* struct timex is 208 bytes; only the fields used here are named */
	struct { unsigned modes; long offset; long rest[24]; } tx = { 0 };
	if (in) {
		if (in->tv_sec > 1000 || in->tv_sec < -1000) {
			errno = EINVAL;
			return -1;
		}
		tx.offset = in->tv_sec * 1000000 + in->tv_usec;
		tx.modes = 0x8001;   /* ADJ_OFFSET_SINGLESHOT */
	}
	if (__syscall_ret((unsigned long)__syscall(SYS_adjtimex, &tx)) < 0) return -1;
	if (out) {
		out->tv_sec = tx.offset / 1000000;
		out->tv_usec = tx.offset % 1000000;
	}
	return 0;
}

clock_t clock(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts)) return (clock_t)-1;
	if (ts.tv_sec > LONG_MAX / 1000000 - 1) return (clock_t)-1;
	return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

int timespec_get(struct timespec *ts, int base)
{
	if (base != TIME_UTC) return 0;
	return clock_gettime(CLOCK_REALTIME, ts) ? 0 : base;
}

int timespec_getres(struct timespec *ts, int base)
{
	if (base != TIME_UTC) return 0;
	if (ts) clock_getres(CLOCK_REALTIME, ts);
	return base;
}

int clock_getcpuclockid(pid_t pid, clockid_t *clk)
{
	struct timespec ts;
	clockid_t id = (clockid_t)((-pid - 1) * 8U + 2);
	long r = __syscall(SYS_clock_getres, id, &ts);
	if (r) return (int)-r;
	*clk = id;
	return 0;
}

unsigned alarm(unsigned sec)
{
	struct itimerval it = { { 0, 0 }, { (time_t)sec, 0 } }, old = { { 0, 0 }, { 0, 0 } };
	__syscall(SYS_setitimer, ITIMER_REAL, &it, &old);
	return (unsigned)(old.it_value.tv_sec + !!old.it_value.tv_usec);
}

int getitimer(int which, struct itimerval *v) { return (int)__sysret(SYS_getitimer, which, v); }
int setitimer(int which, const struct itimerval *restrict v, struct itimerval *restrict old)
{
	return (int)__sysret(SYS_setitimer, which, v, old);
}

double difftime(time_t t1, time_t t0)
{
	long d;
	if (!__builtin_sub_overflow(t1, t0, &d)) return (double)d;
	return (double)t1 - (double)t0;
}
