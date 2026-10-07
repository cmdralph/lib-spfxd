/* lib-spfxd — <sys/sendfile.h> */
#ifndef _SYS_SENDFILE_H
#define _SYS_SENDFILE_H
#include <features.h>
#define __SPFXD_NEED_off_t
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_ssize_t
#include <bits/typedefs.h>
__SPFXD_BEGIN_DECLS
ssize_t sendfile(int, int, off_t *, size_t);
__SPFXD_END_DECLS
#endif
