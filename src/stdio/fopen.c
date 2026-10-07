/* lib-spfxd — fopen / fdopen / mode parsing / stream allocation. */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "stdio_impl.h"
#include "syscall.h"

/* One allocation holds the FILE, the ungetc slack and the buffer; its
 * total size is exactly one allocator size class. */
#define STREAM_ALLOC 8192

hidden int __fmodeflags(const char *mode)
{
	int flags;
	if (strchr(mode, '+')) flags = O_RDWR;
	else if (*mode == 'r') flags = O_RDONLY;
	else flags = O_WRONLY;
	if (strchr(mode, 'x')) flags |= O_EXCL;
	if (strchr(mode, 'e')) flags |= O_CLOEXEC;
	if (*mode != 'r') flags |= O_CREAT;
	if (*mode == 'w') flags |= O_TRUNC;
	if (*mode == 'a') flags |= O_APPEND;
	return flags;
}

hidden FILE *__fdopen_flags(int fd, int flags)
{
	FILE *f = malloc(STREAM_ALLOC);
	if (!f) return 0;
	memset(f, 0, sizeof *f);
	f->fd = fd;
	f->buf = (unsigned char *)(f + 1) + UNGET;
	f->buf_size = f->buf_cap = STREAM_ALLOC - sizeof *f - UNGET;
	f->lbf = EOF;
	if ((flags & O_ACCMODE) == O_RDONLY) f->flags |= F_NOWR;
	else f->flags |= F_TTYCHK;
	if ((flags & O_ACCMODE) == O_WRONLY) f->flags |= F_NORD;
	if (flags & O_APPEND) f->flags |= F_APP;
	f->read = __stdio_read;
	f->write = __stdio_write;
	f->seek = __stdio_seek;
	f->close = __stdio_close;
	return __ofl_add(f);
}

FILE *fopen(const char *restrict path, const char *restrict mode)
{
	if (!*mode || !strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}
	int flags = __fmodeflags(mode);
	int fd = (int)__sysret_cp(SYS_openat, AT_FDCWD, path, flags, 0666);
	if (fd < 0) return 0;
	FILE *f = __fdopen_flags(fd, flags);
	if (!f) __syscall(SYS_close, fd);
	return f;
}

FILE *fdopen(int fd, const char *mode)
{
	if (!*mode || !strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}
	int flags = __fmodeflags(mode);
	long fl = __syscall(SYS_fcntl, fd, F_GETFL);
	if (fl < 0) {
		errno = (int)-fl;
		return 0;
	}
	if (flags & O_CLOEXEC) __syscall(SYS_fcntl, fd, F_SETFD, FD_CLOEXEC);
	if ((flags & O_APPEND) && !(fl & O_APPEND)) __syscall(SYS_fcntl, fd, F_SETFL, fl | O_APPEND);
	return __fdopen_flags(fd, flags);
}
