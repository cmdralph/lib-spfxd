/*
 * lib-spfxd — resolver internals shared by getaddrinfo, getnameinfo and
 * the gethostby* family.
 */
#ifndef _SPFXD_NETDB_IMPL_H
#define _SPFXD_NETDB_IMPL_H

#include <netdb.h>
#include <stdint.h>
#include <sys/socket.h>
#include <stdio.h>
#include "libc.h"

#define MAXNS 3
#define MAXADDRS 48
#define MAXSERVS 2

struct resolv_conf {
	struct sockaddr_storage ns[MAXNS];
	socklen_t nslen[MAXNS];
	int nns;
	char search[256];
	int ndots, timeout, attempts;
};

struct address {
	int family;
	unsigned scopeid;
	uint8_t addr[16];
	int sortkey;
};

struct service {
	uint16_t port;
	unsigned char proto, socktype;
};

hidden int __get_resolv_conf(struct resolv_conf *);
hidden int __dns_mkquery(const char *name, int type, unsigned char *buf, int bufsz);
/* Send n queries to the configured servers; answers[i] receives up to
 * asz bytes, alens[i] its length (0: none).  Returns 0 or an EAI_ code. */
hidden int __dns_send(const unsigned char *const *q, const int *qlens, unsigned char **answers,
	int *alens, int asz, int n, const struct resolv_conf *);
hidden int __dns_parse(const unsigned char *r, int rlen,
	int (*cb)(void *, int, const unsigned char *, int, const unsigned char *, int), void *ctx);
hidden int __dn_expand(const unsigned char *base, const unsigned char *end,
	const unsigned char *src, char *dest, int space);
hidden int __lookup_name(struct address *buf, char *canon, const char *name, int family, int flags);
hidden int __lookup_serv(struct service *buf, const char *name, int proto, int socktype, int flags);
hidden int __lookup_ipliteral(struct address *buf, const char *name, int family);
hidden int __reverse_lookup(const struct address *a, char *host, size_t hostlen, int namereqd);

#endif
