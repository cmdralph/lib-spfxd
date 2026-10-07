/* lib-spfxd — open / openat / creat (cancellation points). */
#include <fcntl.h>
#include <stdarg.h>
#include "syscall.h"

/* The mode argument is only present with O_CREAT or O_TMPFILE. */
#define NEEDS_MODE(fl) (((fl) & O_CREAT) || ((fl) & O_TMPFILE) == O_TMPFILE)

int open(const char *path, int flags, ...)
{
	mode_t mode = 0;
	if (NEEDS_MODE(flags)) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	return (int)__sysret_cp(SYS_openat, AT_FDCWD, path, flags | O_LARGEFILE, mode);
}
weak_alias(open, open64);

int openat(int dirfd, const char *path, int flags, ...)
{
	mode_t mode = 0;
	if (NEEDS_MODE(flags)) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	return (int)__sysret_cp(SYS_openat, dirfd, path, flags | O_LARGEFILE, mode);
}
weak_alias(openat, openat64);

int creat(const char *path, mode_t mode)
{
	return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}
