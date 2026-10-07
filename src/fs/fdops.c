/* lib-spfxd — descriptor management: dup*, pipe*, fcntl, ioctl, flock. */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "syscall.h"

int dup(int fd) { return (int)__sysret(SYS_dup, fd); }

int dup2(int old, int new)
{
	long r;
	if (old == new) {
		/* dup3 rejects equal descriptors; dup2 must validate and return */
		r = __syscall(SYS_fcntl, old, F_GETFD);
		return r < 0 ? (int)__syscall_ret((unsigned long)r) : new;
	}
	while ((r = __syscall(SYS_dup3, old, new, 0)) == -EBUSY);
	return (int)__syscall_ret((unsigned long)r);
}

int dup3(int old, int new, int flags)
{
	long r;
	while ((r = __syscall(SYS_dup3, old, new, flags)) == -EBUSY);
	return (int)__syscall_ret((unsigned long)r);
}

int pipe(int fd[2]) { return (int)__sysret(SYS_pipe2, fd, 0); }
int pipe2(int fd[2], int flags) { return (int)__sysret(SYS_pipe2, fd, flags); }

int fcntl(int fd, int cmd, ...)
{
	va_list ap;
	va_start(ap, cmd);
	unsigned long arg = va_arg(ap, unsigned long);
	va_end(ap);

	if (cmd == F_SETFL) arg |= O_LARGEFILE;
	if (cmd == F_SETLKW) return (int)__sysret_cp(SYS_fcntl, fd, cmd, arg);
	if (cmd == F_GETOWN) {
		/* F_GETOWN cannot report negative (process group) owners
		 * unambiguously; F_GETOWN_EX can. */
		struct { int type; int pid; } ex;
		long r = __syscall(SYS_fcntl, fd, F_GETOWN_EX, &ex);
		if (r == -EINVAL) return (int)__syscall(SYS_fcntl, fd, F_GETOWN);
		if (r) return (int)__syscall_ret((unsigned long)r);
		return ex.type == 2 /* F_OWNER_PGRP */ ? -ex.pid : ex.pid;
	}
	return (int)__sysret(SYS_fcntl, fd, cmd, arg);
}
weak_alias(fcntl, fcntl64);

int ioctl(int fd, unsigned long req, ...)
{
	va_list ap;
	va_start(ap, req);
	void *arg = va_arg(ap, void *);
	va_end(ap);
	return (int)__sysret(SYS_ioctl, fd, req, arg);
}

int flock(int fd, int op) { return (int)__sysret(SYS_flock, fd, op); }

int lockf(int fd, int op, off_t size)
{
	struct flock l = { .l_type = F_WRLCK, .l_whence = SEEK_CUR, .l_len = size };
	switch (op) {
	case F_TEST:
		l.l_type = F_RDLCK;
		if (fcntl(fd, F_GETLK, &l) < 0) return -1;
		if (l.l_type == F_UNLCK || l.l_pid == getpid()) return 0;
		errno = EACCES;
		return -1;
	case F_ULOCK:
		l.l_type = F_UNLCK;
		/* fallthrough */
	case F_TLOCK:
		return fcntl(fd, F_SETLK, &l);
	case F_LOCK:
		return fcntl(fd, F_SETLKW, &l);
	}
	errno = EINVAL;
	return -1;
}

int posix_fadvise(int fd, off_t off, off_t len, int advice)
{
	return (int)-__syscall(SYS_fadvise64, fd, off, len, advice);
}

int posix_fallocate(int fd, off_t off, off_t len)
{
	return (int)-__syscall(SYS_fallocate, fd, 0, off, len);
}

int fallocate(int fd, int mode, off_t off, off_t len)
{
	return (int)__sysret(SYS_fallocate, fd, mode, off, len);
}

ssize_t readahead(int fd, off_t off, size_t n) { return __sysret(SYS_readahead, fd, off, n); }
ssize_t splice(int in, off_t *ioff, int out, off_t *ooff, size_t n, unsigned fl)
{
	return __sysret(SYS_splice, in, ioff, out, ooff, n, fl);
}
ssize_t tee(int in, int out, size_t n, unsigned fl) { return __sysret(SYS_tee, in, out, n, fl); }
ssize_t vmsplice(int fd, const struct iovec *iov, size_t n, unsigned fl) { return __sysret(SYS_vmsplice, fd, iov, n, fl); }
ssize_t copy_file_range(int in, off_t *ioff, int out, off_t *ooff, size_t n, unsigned fl)
{
	return __sysret(SYS_copy_file_range, in, ioff, out, ooff, n, fl);
}
int name_to_handle_at(int dirfd, const char *path, void *h, int *mnt, int flags)
{
	return (int)__sysret(SYS_name_to_handle_at, dirfd, path, h, mnt, flags);
}
