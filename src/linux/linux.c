/* lib-spfxd — Linux-specific system interfaces. */
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/sendfile.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/uio.h>
#include <poll.h>
#include <sys/select.h>
#include <unistd.h>
#include "pthread_impl.h"

int epoll_create1(int flags) { return (int)__sysret(SYS_epoll_create1, flags); }

int epoll_create(int size)
{
	if (size <= 0) {
		errno = EINVAL;
		return -1;
	}
	return epoll_create1(0);
}

int epoll_ctl(int fd, int op, int fd2, struct epoll_event *ev) { return (int)__sysret(SYS_epoll_ctl, fd, op, fd2, ev); }

int epoll_pwait(int fd, struct epoll_event *ev, int cnt, int to, const sigset_t *sigs)
{
	sigset_t m;
	if (sigs) {
		m.__bits[0] = sigs->__bits[0];
		__sig_strip_internal(&m);
		sigs = &m;
	}
	return (int)__sysret_cp(SYS_epoll_pwait, fd, ev, cnt, to, sigs, 8);
}

int epoll_wait(int fd, struct epoll_event *ev, int cnt, int to) { return epoll_pwait(fd, ev, cnt, to, 0); }

int eventfd(unsigned v, int flags) { return (int)__sysret(SYS_eventfd2, v, flags); }
int eventfd_read(int fd, eventfd_t *v) { return read(fd, v, sizeof *v) == sizeof *v ? 0 : -1; }
int eventfd_write(int fd, eventfd_t v) { return write(fd, &v, sizeof v) == sizeof v ? 0 : -1; }

int inotify_init(void) { return (int)__sysret(SYS_inotify_init1, 0); }
int inotify_init1(int flags) { return (int)__sysret(SYS_inotify_init1, flags); }
int inotify_add_watch(int fd, const char *path, uint32_t mask) { return (int)__sysret(SYS_inotify_add_watch, fd, path, mask); }
int inotify_rm_watch(int fd, int wd) { return (int)__sysret(SYS_inotify_rm_watch, fd, wd); }

int signalfd(int fd, const sigset_t *mask, int flags)
{
	sigset_t m;
	m.__bits[0] = mask->__bits[0];
	__sig_strip_internal(&m);
	return (int)__sysret(SYS_signalfd4, fd, &m, 8, flags);
}

int timerfd_create(int clk, int flags) { return (int)__sysret(SYS_timerfd_create, clk, flags); }
int timerfd_settime(int fd, int flags, const struct itimerspec *v, struct itimerspec *old)
{
	return (int)__sysret(SYS_timerfd_settime, fd, flags, v, old);
}
int timerfd_gettime(int fd, struct itimerspec *v) { return (int)__sysret(SYS_timerfd_gettime, fd, v); }

ssize_t sendfile(int out, int in, off_t *off, size_t n) { return __sysret(SYS_sendfile, out, in, off, n); }

int prctl(int op, ...)
{
	unsigned long a[4];
	va_list ap;
	va_start(ap, op);
	for (int i = 0; i < 4; i++) a[i] = va_arg(ap, unsigned long);
	va_end(ap);
	return (int)__sysret(SYS_prctl, op, a[0], a[1], a[2], a[3]);
}

int mount(const char *src, const char *tgt, const char *type, unsigned long fl, const void *data)
{
	return (int)__sysret(SYS_mount, src, tgt, type, fl, data);
}
int umount(const char *tgt) { return (int)__sysret(SYS_umount2, tgt, 0); }
int umount2(const char *tgt, int fl) { return (int)__sysret(SYS_umount2, tgt, fl); }

ssize_t process_vm_readv(pid_t pid, const struct iovec *l, unsigned long ln,
	const struct iovec *r, unsigned long rn, unsigned long fl)
{
	return __sysret(SYS_process_vm_readv, pid, l, ln, r, rn, fl);
}

ssize_t process_vm_writev(pid_t pid, const struct iovec *l, unsigned long ln,
	const struct iovec *r, unsigned long rn, unsigned long fl)
{
	return __sysret(SYS_process_vm_writev, pid, l, ln, r, rn, fl);
}

int poll(struct pollfd *fds, nfds_t n, int timeout)
{
	struct timespec ts, *tp = 0;
	if (timeout >= 0) {
		ts.tv_sec = timeout / 1000;
		ts.tv_nsec = timeout % 1000 * 1000000L;
		tp = &ts;
	}
	return (int)__sysret_cp(SYS_ppoll, fds, n, tp, 0, 8);
}

int ppoll(struct pollfd *fds, nfds_t n, const struct timespec *to, const sigset_t *mask)
{
	sigset_t m;
	if (mask) {
		m.__bits[0] = mask->__bits[0];
		__sig_strip_internal(&m);
		mask = &m;
	}
	/* the kernel updates the timeout: work on a copy */
	struct timespec t, *tp = 0;
	if (to) {
		t = *to;
		tp = &t;
	}
	return (int)__sysret_cp(SYS_ppoll, fds, n, tp, mask, 8);
}

int select(int n, fd_set *restrict r, fd_set *restrict w, fd_set *restrict e, struct timeval *restrict tv)
{
	struct timespec ts, *tp = 0;
	if (tv) {
		if (tv->tv_sec < 0 || tv->tv_usec < 0) {
			errno = EINVAL;
			return -1;
		}
		ts.tv_sec = tv->tv_sec + tv->tv_usec / 1000000;
		ts.tv_nsec = tv->tv_usec % 1000000 * 1000;
		tp = &ts;
	}
	int rr = (int)__sysret_cp(SYS_pselect6, n, r, w, e, tp, 0);
	if (tv && rr >= 0) {
		/* Linux reports the time remaining */
		tv->tv_sec = ts.tv_sec;
		tv->tv_usec = ts.tv_nsec / 1000;
	}
	return rr;
}

int pselect(int n, fd_set *restrict r, fd_set *restrict w, fd_set *restrict e,
	const struct timespec *restrict to, const sigset_t *restrict mask)
{
	sigset_t m;
	struct { const sigset_t *ss; size_t len; } data = { 0, 8 };
	if (mask) {
		m.__bits[0] = mask->__bits[0];
		__sig_strip_internal(&m);
		data.ss = &m;
	}
	struct timespec t, *tp = 0;
	if (to) {
		t = *to;
		tp = &t;
	}
	return (int)__sysret_cp(SYS_pselect6, n, r, w, e, tp, mask ? &data : 0);
}
