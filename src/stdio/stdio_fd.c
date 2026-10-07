/*
 * lib-spfxd — file-descriptor backed stream operations.
 *
 * Writing combines the pending buffer and the caller's data into a single
 * writev() so a full buffer plus a large fwrite costs one system call.
 * Interrupted or failed I/O is reported, never silently retried: a signal
 * handler installed without SA_RESTART is meant to interrupt blocking
 * stdio, and POSIX requires the error indicator and errno in that case.
 */
#include <errno.h>
#include <unistd.h>
#include <sys/uio.h>
#include "stdio_impl.h"
#include "syscall.h"

hidden size_t __stdio_read(FILE *f, unsigned char *dst, size_t n)
{
	long r = __syscall_cp(SYS_read, f->fd, dst, n);
	if (r <= 0) {
		if (r == 0) f->flags |= F_EOF;
		else {
			f->flags |= F_ERR;
			errno = (int)-r;
		}
		return 0;
	}
	return (size_t)r;
}

hidden size_t __stdio_write(FILE *f, const unsigned char *buf, size_t len)
{
	struct iovec iovs[2] = {
		{ .iov_base = f->wbase, .iov_len = (size_t)(f->wpos - f->wbase) },
		{ .iov_base = (void *)buf, .iov_len = len },
	};
	struct iovec *iov = iovs;
	int iovcnt = 2;
	size_t rem = iovs[0].iov_len + len;

	if (!iovs[0].iov_len) { iov++; iovcnt = 1; }
	while (rem) {
		long cnt = __syscall_cp(SYS_writev, f->fd, iov, iovcnt);
		if (cnt <= 0) {
			f->wpos = f->wbase = f->wend = 0;
			f->flags |= F_ERR;
			errno = cnt ? (int)-cnt : EIO;
			/* report how much of the caller's data went out */
			return iovcnt == 2 ? 0 : len - iov[0].iov_len;
		}
		rem -= (size_t)cnt;
		while (iovcnt && (size_t)cnt >= iov[0].iov_len) {
			cnt -= (long)iov[0].iov_len;
			iov++;
			iovcnt--;
		}
		if (iovcnt) {
			iov[0].iov_base = (char *)iov[0].iov_base + cnt;
			iov[0].iov_len -= (size_t)cnt;
		}
	}
	f->wpos = f->wbase = f->buf;
	f->wend = f->buf + f->buf_size;
	return len;
}

hidden off_t __stdio_seek(FILE *f, off_t off, int whence)
{
	return (off_t)__sysret(SYS_lseek, f->fd, off, whence);
}

hidden int __stdio_close(FILE *f)
{
	long r = __syscall_cp(SYS_close, f->fd);
	if (r == -EINTR) r = 0;    /* Linux always releases the descriptor */
	return (int)__syscall_ret((unsigned long)r);
}
