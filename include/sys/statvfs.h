/* lib-spfxd — <sys/statvfs.h> */
#ifndef _SYS_STATVFS_H
#define _SYS_STATVFS_H
#include <features.h>
#define __SPFXD_NEED_fsblkcnt_t
#include <bits/typedefs.h>
struct statvfs {
	unsigned long f_bsize, f_frsize;
	fsblkcnt_t f_blocks, f_bfree, f_bavail;
	fsfilcnt_t f_files, f_ffree, f_favail;
	unsigned long f_fsid, f_flag, f_namemax;
	int __reserved[6];
};
#define ST_RDONLY 1
#define ST_NOSUID 2
#define ST_NODEV  4
#define ST_NOEXEC 8
#define ST_SYNCHRONOUS 16
#define ST_MANDLOCK 64
#define ST_NOATIME 1024
#define ST_NODIRATIME 2048
#define ST_RELATIME 4096
__SPFXD_BEGIN_DECLS
int statvfs(const char *__restrict, struct statvfs *__restrict);
int fstatvfs(int, struct statvfs *);
__SPFXD_END_DECLS
#endif
