/* lib-spfxd — <net/if.h> */
#ifndef _NET_IF_H
#define _NET_IF_H
#include <features.h>

__SPFXD_BEGIN_DECLS

#define IF_NAMESIZE 16

struct if_nameindex {
	unsigned int if_index;
	char *if_name;
};

unsigned int if_nametoindex(const char *);
char *if_indextoname(unsigned int, char *);
struct if_nameindex *if_nameindex(void);
void if_freenameindex(struct if_nameindex *);

#ifdef __SPFXD_GNU
#define IFNAMSIZ IF_NAMESIZE
#endif

__SPFXD_END_DECLS
#endif
