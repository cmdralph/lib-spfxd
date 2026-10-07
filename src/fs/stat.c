/* lib-spfxd — stat family. */
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <string.h>
#include "syscall.h"

int fstatat(int fd, const char *restrict path, struct stat *restrict st, int flag)
{
	return (int)__sysret(SYS_newfstatat, fd, path, st, flag);
}
weak_alias(fstatat, fstatat64);

int stat(const char *restrict path, struct stat *restrict st)
{
	return fstatat(AT_FDCWD, path, st, 0);
}
weak_alias(stat, stat64);

int lstat(const char *restrict path, struct stat *restrict st)
{
	return fstatat(AT_FDCWD, path, st, AT_SYMLINK_NOFOLLOW);
}
weak_alias(lstat, lstat64);

int fstat(int fd, struct stat *st)
{
	long r = __syscall(SYS_fstat, fd, st);
	if (r == -9 /* EBADF */ || r >= 0) return (int)__syscall_ret((unsigned long)r);
	/* descriptors opened with O_PATH */
	return fstatat(fd, "", st, AT_EMPTY_PATH);
}
weak_alias(fstat, fstat64);

int statfs(const char *path, struct statfs *buf)
{
	return (int)__sysret(SYS_statfs, path, buf);
}

int fstatfs(int fd, struct statfs *buf)
{
	return (int)__sysret(SYS_fstatfs, fd, buf);
}

static void to_statvfs(struct statvfs *v, const struct statfs *f)
{
	memset(v, 0, sizeof *v);
	v->f_bsize = (unsigned long)f->f_bsize;
	v->f_frsize = (unsigned long)(f->f_frsize ? f->f_frsize : f->f_bsize);
	v->f_blocks = f->f_blocks;
	v->f_bfree = f->f_bfree;
	v->f_bavail = f->f_bavail;
	v->f_files = f->f_files;
	v->f_ffree = f->f_ffree;
	v->f_favail = f->f_ffree;
	v->f_fsid = (unsigned long)(unsigned)f->f_fsid.__val[0];
	v->f_flag = (unsigned long)f->f_flags;
	v->f_namemax = (unsigned long)f->f_namelen;
}

int statvfs(const char *restrict path, struct statvfs *restrict v)
{
	struct statfs f;
	if (statfs(path, &f) < 0) return -1;
	to_statvfs(v, &f);
	return 0;
}

int fstatvfs(int fd, struct statvfs *v)
{
	struct statfs f;
	if (fstatfs(fd, &f) < 0) return -1;
	to_statvfs(v, &f);
	return 0;
}
