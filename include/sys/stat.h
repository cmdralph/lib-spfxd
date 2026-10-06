/* lib-spfxd — <sys/stat.h> */
#ifndef _SYS_STAT_H
#define _SYS_STAT_H
#include <features.h>

#define __SPFXD_NEED_dev_t
#define __SPFXD_NEED_ino_t
#define __SPFXD_NEED_mode_t
#define __SPFXD_NEED_nlink_t
#define __SPFXD_NEED_uid_t
#define __SPFXD_NEED_gid_t
#define __SPFXD_NEED_off_t
#define __SPFXD_NEED_time_t
#define __SPFXD_NEED_blksize_t
#define __SPFXD_NEED_blkcnt_t
#define __SPFXD_NEED_struct_timespec
#include <bits/typedefs.h>
#include <bits/stat.h>

#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFBLK  0060000
#define S_IFREG  0100000
#define S_IFIFO  0010000
#define S_IFLNK  0120000
#define S_IFSOCK 0140000
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#define S_TYPEISMQ(b)  0
#define S_TYPEISSEM(b) 0
#define S_TYPEISSHM(b) 0
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXU 0700
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXG 0070
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_IRWXO 0007
#if defined(__SPFXD_BSD)
#define S_IREAD  S_IRUSR
#define S_IWRITE S_IWUSR
#define S_IEXEC  S_IXUSR
#define ACCESSPERMS (S_IRWXU|S_IRWXG|S_IRWXO)
#define ALLPERMS (S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)
#define DEFFILEMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif

#define UTIME_NOW  ((1l << 30) - 1l)
#define UTIME_OMIT ((1l << 30) - 2l)

__SPFXD_BEGIN_DECLS
int stat(const char *__restrict, struct stat *__restrict);
int fstat(int, struct stat *);
int lstat(const char *__restrict, struct stat *__restrict);
int fstatat(int, const char *__restrict, struct stat *__restrict, int);
int chmod(const char *, mode_t);
int fchmod(int, mode_t);
int fchmodat(int, const char *, mode_t, int);
mode_t umask(mode_t);
int mkdir(const char *, mode_t);
int mkdirat(int, const char *, mode_t);
int mkfifo(const char *, mode_t);
int mkfifoat(int, const char *, mode_t);
int mknod(const char *, mode_t, dev_t);
int mknodat(int, const char *, mode_t, dev_t);
int futimens(int, const struct timespec[2]);
int utimensat(int, const char *, const struct timespec[2], int);
#if defined(__SPFXD_GNU)
int lchmod(const char *, mode_t);
#endif
__SPFXD_END_DECLS
#endif
