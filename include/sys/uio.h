/* lib-spfxd — <sys/uio.h> */
#ifndef _SYS_UIO_H
#define _SYS_UIO_H
#include <features.h>
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_ssize_t
#define __SPFXD_NEED_off_t
#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_struct_iovec
#include <bits/typedefs.h>
#define UIO_MAXIOV 1024
__SPFXD_BEGIN_DECLS
ssize_t readv(int, const struct iovec *, int);
ssize_t writev(int, const struct iovec *, int);
ssize_t preadv(int, const struct iovec *, int, off_t);
ssize_t pwritev(int, const struct iovec *, int, off_t);
#if defined(__SPFXD_GNU)
ssize_t process_vm_readv(pid_t, const struct iovec *, unsigned long, const struct iovec *, unsigned long, unsigned long);
ssize_t process_vm_writev(pid_t, const struct iovec *, unsigned long, const struct iovec *, unsigned long, unsigned long);
#endif
__SPFXD_END_DECLS
#endif
