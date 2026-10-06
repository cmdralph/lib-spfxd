/* lib-spfxd — descriptor I/O wrappers (cancellation points). */
#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>
#include "syscall.h"

ssize_t read(int fd, void *buf, size_t n) { return __sysret_cp(SYS_read, fd, buf, n); }
ssize_t write(int fd, const void *buf, size_t n) { return __sysret_cp(SYS_write, fd, buf, n); }
ssize_t pread(int fd, void *buf, size_t n, off_t off) { return __sysret_cp(SYS_pread64, fd, buf, n, off); }
ssize_t pwrite(int fd, const void *buf, size_t n, off_t off) { return __sysret_cp(SYS_pwrite64, fd, buf, n, off); }
ssize_t readv(int fd, const struct iovec *iov, int cnt) { return __sysret_cp(SYS_readv, fd, iov, cnt); }
ssize_t writev(int fd, const struct iovec *iov, int cnt) { return __sysret_cp(SYS_writev, fd, iov, cnt); }

/* The kernel takes the 64-bit offset split into low/high halves. */
ssize_t preadv(int fd, const struct iovec *iov, int cnt, off_t off)
{
	return __sysret_cp(SYS_preadv, fd, iov, cnt, off, 0);
}

ssize_t pwritev(int fd, const struct iovec *iov, int cnt, off_t off)
{
	return __sysret_cp(SYS_pwritev, fd, iov, cnt, off, 0);
}

weak_alias(pread, pread64);
weak_alias(pwrite, pwrite64);

int close(int fd)
{
	long r = __syscall_cp(SYS_close, fd);
	/* Linux releases the descriptor even when close is interrupted;
	 * retrying could close a descriptor another thread just opened. */
	if (r == -EINTR) r = 0;
	return (int)__syscall_ret((unsigned long)r);
}

off_t lseek(int fd, off_t off, int whence)
{
	return (off_t)__sysret(SYS_lseek, fd, off, whence);
}
weak_alias(lseek, lseek64);
