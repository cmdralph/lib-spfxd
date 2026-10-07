/* lib-spfxd — <fcntl.h> */
#ifndef _FCNTL_H
#define _FCNTL_H
#include <features.h>

#define __SPFXD_NEED_off_t
#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_mode_t
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_ssize_t
#define __SPFXD_NEED_struct_iovec
#include <bits/typedefs.h>
#include <bits/fcntl.h>

#define O_RDONLY  00
#define O_WRONLY  01
#define O_RDWR    02
#define O_ACCMODE 03
#define O_SEARCH  O_PATH
#define O_EXEC    O_PATH

#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4
#define F_GETLK  5
#define F_SETLK  6
#define F_SETLKW 7
#define F_SETOWN 8
#define F_GETOWN 9
#define F_SETSIG 10
#define F_GETSIG 11
#define F_SETOWN_EX 15
#define F_GETOWN_EX 16
#define F_OFD_GETLK  36
#define F_OFD_SETLK  37
#define F_OFD_SETLKW 38
#define F_DUPFD_CLOEXEC 1030
#define F_SETPIPE_SZ 1031
#define F_GETPIPE_SZ 1032
#define F_ADD_SEALS  1033
#define F_GET_SEALS  1034
#define F_SEAL_SEAL   0x0001
#define F_SEAL_SHRINK 0x0002
#define F_SEAL_GROW   0x0004
#define F_SEAL_WRITE  0x0008

#define FD_CLOEXEC 1
#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2

#define AT_FDCWD            (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR        0x200
#define AT_SYMLINK_FOLLOW   0x400
#define AT_EACCESS          0x200
#define AT_NO_AUTOMOUNT     0x800
#define AT_EMPTY_PATH       0x1000

#define POSIX_FADV_NORMAL     0
#define POSIX_FADV_RANDOM     1
#define POSIX_FADV_SEQUENTIAL 2
#define POSIX_FADV_WILLNEED   3
#define POSIX_FADV_DONTNEED   4
#define POSIX_FADV_NOREUSE    5

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFBLK  0060000
#define S_IFREG  0100000
#define S_IFIFO  0010000
#define S_IFLNK  0120000
#define S_IFSOCK 0140000
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

struct flock {
	short l_type;
	short l_whence;
	off_t l_start;
	off_t l_len;
	pid_t l_pid;
};

__SPFXD_BEGIN_DECLS
int creat(const char *, mode_t);
int fcntl(int, int, ...);
int open(const char *, int, ...);
int openat(int, const char *, int, ...);
int posix_fadvise(int, off_t, off_t, int);
int posix_fallocate(int, off_t, off_t);
#if defined(__SPFXD_GNU)
#define SPLICE_F_MOVE 1
#define SPLICE_F_NONBLOCK 2
#define SPLICE_F_MORE 4
#define SPLICE_F_GIFT 8
#define FALLOC_FL_KEEP_SIZE 1
#define FALLOC_FL_PUNCH_HOLE 2
int fallocate(int, int, off_t, off_t);
ssize_t readahead(int, off_t, size_t);
ssize_t splice(int, off_t *, int, off_t *, size_t, unsigned);
ssize_t tee(int, int, size_t, unsigned);
ssize_t vmsplice(int, const struct iovec *, size_t, unsigned);
int name_to_handle_at(int, const char *, void *, int *, int);
#endif
__SPFXD_END_DECLS
#endif
