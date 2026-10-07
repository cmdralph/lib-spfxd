/* lib-spfxd — path-based filesystem operations. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <utime.h>
#include "syscall.h"

int access(const char *path, int mode) { return (int)__sysret(SYS_faccessat, AT_FDCWD, path, mode); }

int faccessat(int fd, const char *path, int mode, int flag)
{
	if (!flag) return (int)__sysret(SYS_faccessat, fd, path, mode);
	long r = __syscall(SYS_faccessat2, fd, path, mode, flag);
	return (int)__syscall_ret((unsigned long)r);
}

int euidaccess(const char *path, int mode) { return faccessat(AT_FDCWD, path, mode, AT_EACCESS); }
weak_alias(euidaccess, eaccess);

int chmod(const char *path, mode_t mode) { return (int)__sysret(SYS_fchmodat, AT_FDCWD, path, mode); }
int fchmod(int fd, mode_t mode) { return (int)__sysret(SYS_fchmod, fd, mode); }

int fchmodat(int fd, const char *path, mode_t mode, int flag)
{
	if (!flag) return (int)__sysret(SYS_fchmodat, fd, path, mode);
	long r = __syscall(SYS_fchmodat2, fd, path, mode, flag);
	return (int)__syscall_ret((unsigned long)r);
}

int lchmod(const char *path, mode_t mode) { return fchmodat(AT_FDCWD, path, mode, AT_SYMLINK_NOFOLLOW); }

int chown(const char *path, uid_t u, gid_t g) { return (int)__sysret(SYS_fchownat, AT_FDCWD, path, u, g, 0); }
int fchown(int fd, uid_t u, gid_t g) { return (int)__sysret(SYS_fchown, fd, u, g); }
int lchown(const char *path, uid_t u, gid_t g) { return (int)__sysret(SYS_fchownat, AT_FDCWD, path, u, g, AT_SYMLINK_NOFOLLOW); }
int fchownat(int fd, const char *path, uid_t u, gid_t g, int flag) { return (int)__sysret(SYS_fchownat, fd, path, u, g, flag); }

int mkdir(const char *path, mode_t mode) { return (int)__sysret(SYS_mkdirat, AT_FDCWD, path, mode); }
int mkdirat(int fd, const char *path, mode_t mode) { return (int)__sysret(SYS_mkdirat, fd, path, mode); }
int rmdir(const char *path) { return (int)__sysret(SYS_unlinkat, AT_FDCWD, path, AT_REMOVEDIR); }
int unlink(const char *path) { return (int)__sysret(SYS_unlinkat, AT_FDCWD, path, 0); }
int unlinkat(int fd, const char *path, int flag) { return (int)__sysret(SYS_unlinkat, fd, path, flag); }

int remove(const char *path)
{
	long r = __syscall(SYS_unlinkat, AT_FDCWD, path, 0);
	if (r == -EISDIR) r = __syscall(SYS_unlinkat, AT_FDCWD, path, AT_REMOVEDIR);
	return (int)__syscall_ret((unsigned long)r);
}

int rename(const char *old, const char *new) { return (int)__sysret(SYS_renameat2, AT_FDCWD, old, AT_FDCWD, new, 0); }
int renameat(int ofd, const char *old, int nfd, const char *new) { return (int)__sysret(SYS_renameat2, ofd, old, nfd, new, 0); }

int link(const char *old, const char *new) { return (int)__sysret(SYS_linkat, AT_FDCWD, old, AT_FDCWD, new, 0); }
int linkat(int ofd, const char *old, int nfd, const char *new, int flag)
{
	return (int)__sysret(SYS_linkat, ofd, old, nfd, new, flag);
}

int symlink(const char *target, const char *path) { return (int)__sysret(SYS_symlinkat, target, AT_FDCWD, path); }
int symlinkat(const char *target, int fd, const char *path) { return (int)__sysret(SYS_symlinkat, target, fd, path); }

ssize_t readlink(const char *restrict path, char *restrict buf, size_t n)
{
	return readlinkat(AT_FDCWD, path, buf, n);
}

ssize_t readlinkat(int fd, const char *restrict path, char *restrict buf, size_t n)
{
	char dummy[1];
	if (!n) {
		/* POSIX treats bufsize 0 as an error; Linux would return EINVAL */
		buf = dummy;
		n = 1;
	}
	return __sysret(SYS_readlinkat, fd, path, buf, n);
}

int mknod(const char *path, mode_t mode, dev_t dev) { return (int)__sysret(SYS_mknodat, AT_FDCWD, path, mode, dev); }
int mknodat(int fd, const char *path, mode_t mode, dev_t dev) { return (int)__sysret(SYS_mknodat, fd, path, mode, dev); }
int mkfifo(const char *path, mode_t mode) { return mknod(path, mode | S_IFIFO, 0); }
int mkfifoat(int fd, const char *path, mode_t mode) { return mknodat(fd, path, mode | S_IFIFO, 0); }

mode_t umask(mode_t mode) { return (mode_t)__syscall(SYS_umask, mode & 0777); }

int truncate(const char *path, off_t len) { return (int)__sysret(SYS_truncate, path, len); }
int ftruncate(int fd, off_t len) { return (int)__sysret(SYS_ftruncate, fd, len); }
weak_alias(truncate, truncate64);
weak_alias(ftruncate, ftruncate64);

int fsync(int fd) { return (int)__sysret_cp(SYS_fsync, fd); }
int fdatasync(int fd) { return (int)__sysret_cp(SYS_fdatasync, fd); }
void sync(void) { __syscall(SYS_sync); }
int syncfs(int fd) { return (int)__sysret(SYS_syncfs, fd); }

int utimensat(int fd, const char *path, const struct timespec times[2], int flag)
{
	return (int)__sysret(SYS_utimensat, fd, path, times, flag);
}

int futimens(int fd, const struct timespec times[2])
{
	return utimensat(fd, 0, times, 0);
}

static int tv_to_ts(const struct timeval tv[2], struct timespec ts[2])
{
	if (!tv) return 0;
	for (int i = 0; i < 2; i++) {
		if ((unsigned long)tv[i].tv_usec >= 1000000UL) {
			errno = EINVAL;
			return -1;
		}
		ts[i].tv_sec = tv[i].tv_sec;
		ts[i].tv_nsec = tv[i].tv_usec * 1000;
	}
	return 1;
}

int utimes(const char *path, const struct timeval tv[2])
{
	struct timespec ts[2];
	int r = tv_to_ts(tv, ts);
	if (r < 0) return -1;
	return utimensat(AT_FDCWD, path, r ? ts : 0, 0);
}

int lutimes(const char *path, const struct timeval tv[2])
{
	struct timespec ts[2];
	int r = tv_to_ts(tv, ts);
	if (r < 0) return -1;
	return utimensat(AT_FDCWD, path, r ? ts : 0, AT_SYMLINK_NOFOLLOW);
}

int futimes(int fd, const struct timeval tv[2])
{
	struct timespec ts[2];
	int r = tv_to_ts(tv, ts);
	if (r < 0) return -1;
	return futimens(fd, r ? ts : 0);
}

int futimesat(int dirfd, const char *path, const struct timeval tv[2])
{
	struct timespec ts[2];
	int r = tv_to_ts(tv, ts);
	if (r < 0) return -1;
	return utimensat(dirfd, path, r ? ts : 0, 0);
}

int utime(const char *path, const struct utimbuf *t)
{
	if (!t) return utimensat(AT_FDCWD, path, 0, 0);
	struct timespec ts[2] = { { t->actime, 0 }, { t->modtime, 0 } };
	return utimensat(AT_FDCWD, path, ts, 0);
}

int chdir(const char *path) { return (int)__sysret(SYS_chdir, path); }
int fchdir(int fd) { return (int)__sysret(SYS_fchdir, fd); }
int chroot(const char *path) { return (int)__sysret(SYS_chroot, path); }
int acct(const char *path) { return (int)__sysret(SYS_acct, path); }
