/* lib-spfxd — <libgen.h> */
#ifndef _LIBGEN_H
#define _LIBGEN_H
#include <features.h>
__SPFXD_BEGIN_DECLS
char *dirname(char *);
char *__xpg_basename(char *);
#define basename __xpg_basename
__SPFXD_END_DECLS
#endif
