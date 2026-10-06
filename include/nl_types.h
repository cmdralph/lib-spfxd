/* lib-spfxd — <nl_types.h> (message catalogs) */
#ifndef _NL_TYPES_H
#define _NL_TYPES_H
#include <features.h>
#define NL_SETD 1
#define NL_CAT_LOCALE 1
typedef int nl_item;
typedef void *nl_catd;
__SPFXD_BEGIN_DECLS
nl_catd catopen(const char *, int);
char *catgets(nl_catd, int, int, const char *);
int catclose(nl_catd);
__SPFXD_END_DECLS
#endif
