/* lib-spfxd — <sys/statfs.h> (kernel struct statfs, x86-64) */
#ifndef _SYS_STATFS_H
#define _SYS_STATFS_H
#include <features.h>
#define __SPFXD_NEED_fsblkcnt_t
#include <bits/typedefs.h>
typedef struct { int __val[2]; } fsid_t;
struct statfs {
	long f_type, f_bsize;
	fsblkcnt_t f_blocks, f_bfree, f_bavail;
	fsfilcnt_t f_files, f_ffree;
	fsid_t f_fsid;
	long f_namelen, f_frsize, f_flags, f_spare[4];
};
__SPFXD_BEGIN_DECLS
int statfs(const char *, struct statfs *);
int fstatfs(int, struct statfs *);
__SPFXD_END_DECLS
#endif
