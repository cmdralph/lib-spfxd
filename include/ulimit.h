/* lib-spfxd — <ulimit.h> (obsolescent; use getrlimit/setrlimit) */
#ifndef _ULIMIT_H
#define _ULIMIT_H
#include <features.h>
#define UL_GETFSIZE 1
#define UL_SETFSIZE 2
__SPFXD_BEGIN_DECLS
long ulimit(int, ...);
__SPFXD_END_DECLS
#endif
