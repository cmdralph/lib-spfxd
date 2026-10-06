/* lib-spfxd — <sys/file.h> */
#ifndef _SYS_FILE_H
#define _SYS_FILE_H
#include <features.h>
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8
__SPFXD_BEGIN_DECLS
int flock(int, int);
__SPFXD_END_DECLS
#endif
