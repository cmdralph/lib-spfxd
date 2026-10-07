/*
 * lib-spfxd — freopen.  The new file is installed on the stream's existing
 * descriptor number (dup3), so freopen(..., stdout) keeps fd 1.  With a
 * NULL path only the access mode flags that Linux can change on an open
 * descriptor (O_APPEND, O_CLOEXEC) are applied.
 */
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "stdio_impl.h"
#include "syscall.h"

FILE *freopen(const char *restrict path, const char *restrict mode, FILE *restrict f)
{
	int flags = __fmodeflags(mode);
	FLOCK(f);
	__fflush_unlocked(f);

	if (!path) {
		if (flags & O_CLOEXEC) __syscall(SYS_fcntl, f->fd, F_SETFD, FD_CLOEXEC);
		flags &= ~(O_CREAT | O_EXCL | O_CLOEXEC | O_TRUNC);
		if (__syscall_ret((unsigned long)__syscall(SYS_fcntl, f->fd, F_SETFL, flags)) < 0)
			goto fail;
	} else {
		int fd = (int)__sysret_cp(SYS_open, path, flags, 0666);
		if (fd < 0) goto fail;
		if (fd != f->fd) {
			long r = __syscall(SYS_dup3, fd, f->fd, flags & O_CLOEXEC);
			__syscall(SYS_close, fd);
			if (r < 0) {
				errno = (int)-r;
				goto fail;
			}
		}
	}
	f->flags = (f->flags & (F_PERM | F_ABUF | F_SVB)) | F_TTYCHK;
	if ((flags & O_ACCMODE) == O_RDONLY) f->flags |= F_NOWR;
	if ((flags & O_ACCMODE) == O_WRONLY) f->flags |= F_NORD;
	if (flags & O_APPEND) f->flags |= F_APP;
	if ((flags & O_ACCMODE) == O_RDONLY) f->flags &= ~F_TTYCHK;
	f->mode = 0;
	memset(&f->mbs, 0, sizeof f->mbs);
	f->rpos = f->rend = f->wpos = f->wbase = f->wend = 0;
	f->lbf = EOF;
	FUNLOCK(f);
	return f;
fail:
	FUNLOCK(f);
	fclose(f);
	return 0;
}
