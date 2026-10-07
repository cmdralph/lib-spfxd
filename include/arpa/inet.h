/* lib-spfxd — <arpa/inet.h> */
#ifndef _ARPA_INET_H
#define _ARPA_INET_H
#include <features.h>
#include <netinet/in.h>
__SPFXD_BEGIN_DECLS
in_addr_t inet_addr(const char *);
in_addr_t inet_network(const char *);
char *inet_ntoa(struct in_addr);
int inet_pton(int, const char *__restrict, void *__restrict);
const char *inet_ntop(int, const void *__restrict, char *__restrict, socklen_t);
int inet_aton(const char *, struct in_addr *);
struct in_addr inet_makeaddr(in_addr_t, in_addr_t);
in_addr_t inet_lnaof(struct in_addr);
in_addr_t inet_netof(struct in_addr);
__SPFXD_END_DECLS
#endif
