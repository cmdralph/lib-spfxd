/* lib-spfxd — <fmtmsg.h> */
#ifndef _FMTMSG_H
#define _FMTMSG_H
#include <features.h>

#define MM_HARD    0x001
#define MM_SOFT    0x002
#define MM_FIRM    0x004
#define MM_APPL    0x008
#define MM_UTIL    0x010
#define MM_OPSYS   0x020
#define MM_RECOVER 0x040
#define MM_NRECOV  0x080
#define MM_PRINT   0x100
#define MM_CONSOLE 0x200

#define MM_NULLSEV 0
#define MM_HALT    1
#define MM_ERROR   2
#define MM_WARNING 3
#define MM_INFO    4

#define MM_NULLLBL ((char *)0)
#define MM_NULLTXT ((char *)0)
#define MM_NULLACT ((char *)0)
#define MM_NULLTAG ((char *)0)
#define MM_NULLMC  0L

#define MM_OK     0
#define MM_NOTOK  -1
#define MM_NOMSG  1
#define MM_NOCON  4

__SPFXD_BEGIN_DECLS
int fmtmsg(long, const char *, int, const char *, const char *, const char *);
__SPFXD_END_DECLS
#endif
