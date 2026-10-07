/*
 * lib-spfxd — getaddrinfo, freeaddrinfo, gai_strerror, getnameinfo.
 *
 * Results are produced for every combination of address and service
 * (stream/TCP and datagram/UDP unless the hints restrict them).  Each
 * result node is one allocation holding the addrinfo, its socket address
 * and, on the first node with AI_CANONNAME, the canonical name.
 */
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "netdb_impl.h"

struct aibuf {
	struct addrinfo ai;
	union {
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} sa;
	char canon[];
};

/* Does the system have a route for this family (AI_ADDRCONFIG)?  A UDP
 * connect sends nothing but fails without a usable source address. */
static int family_configured(int af)
{
	int fd = socket(af, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
	if (fd < 0) return 0;
	int ok;
	if (af == AF_INET) {
		struct sockaddr_in s = { .sin_family = AF_INET, .sin_port = htons(53) };
		s.sin_addr.s_addr = htonl(0xc0000201);      /* 192.0.2.1, documentation net */
		ok = !connect(fd, (void *)&s, sizeof s);
	} else {
		struct sockaddr_in6 s = { .sin6_family = AF_INET6, .sin6_port = htons(53) };
		s.sin6_addr.s6_addr[0] = 0x20;
		s.sin6_addr.s6_addr[1] = 0x01;
		s.sin6_addr.s6_addr[2] = 0x0d;
		s.sin6_addr.s6_addr[3] = 0xb8;            /* 2001:db8::1 */
		s.sin6_addr.s6_addr[15] = 1;
		ok = !connect(fd, (void *)&s, sizeof s);
	}
	close(fd);
	return ok;
}

int getaddrinfo(const char *restrict host, const char *restrict serv,
                const struct addrinfo *restrict hint, struct addrinfo **restrict res)
{
	int flags = 0, family = AF_UNSPEC, socktype = 0, proto = 0;
	struct address addrs[MAXADDRS];
	struct service servs[MAXSERVS];
	char canon[256];
	int naddrs, nservs;

	if (!host && !serv) return EAI_NONAME;
	if (hint) {
		flags = hint->ai_flags;
		family = hint->ai_family;
		socktype = hint->ai_socktype;
		proto = hint->ai_protocol;
		const int mask = AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST | AI_V4MAPPED |
		                 AI_ALL | AI_ADDRCONFIG | AI_NUMERICSERV;
		if (flags & ~mask) return EAI_BADFLAGS;
		if ((flags & AI_CANONNAME) && !host) return EAI_BADFLAGS;
	}
	if (family != AF_UNSPEC && family != AF_INET && family != AF_INET6) return EAI_FAMILY;
	if (socktype && socktype != SOCK_STREAM && socktype != SOCK_DGRAM && socktype != SOCK_RAW &&
	    socktype != SOCK_SEQPACKET)
		return EAI_SOCKTYPE;
	if (socktype == SOCK_SEQPACKET) return EAI_SOCKTYPE;

	if (flags & AI_ADDRCONFIG) {
		int has4 = family_configured(AF_INET), has6 = family_configured(AF_INET6);
		if (family == AF_INET && !has4) return EAI_NONAME;
		if (family == AF_INET6 && !has6) return EAI_NONAME;
		if (family == AF_UNSPEC) {
			if (has4 && !has6) family = AF_INET;
			else if (has6 && !has4) family = AF_INET6;
		}
	}

	nservs = __lookup_serv(servs, serv, proto, socktype, flags);
	if (nservs < 0) return nservs;

	int qfamily = family;
	if (family == AF_INET6 && (flags & AI_V4MAPPED)) qfamily = AF_UNSPEC;
	canon[0] = 0;
	if (!host) {
		naddrs = 0;
		if (family != AF_INET) {
			memset(&addrs[naddrs], 0, sizeof addrs[0]);
			addrs[naddrs].family = AF_INET6;
			if (!(flags & AI_PASSIVE)) addrs[naddrs].addr[15] = 1;
			naddrs++;
		}
		if (family != AF_INET6) {
			memset(&addrs[naddrs], 0, sizeof addrs[0]);
			addrs[naddrs].family = AF_INET;
			if (!(flags & AI_PASSIVE)) memcpy(addrs[naddrs].addr, "\x7f\0\0\x01", 4);
			naddrs++;
		}
	} else {
		naddrs = __lookup_name(addrs, canon, host, qfamily, flags);
		if (naddrs < 0) return naddrs;
		if (!naddrs) return EAI_NONAME;
	}
	if (qfamily != family) {
		/* AI_V4MAPPED: IPv6 results, or mapped IPv4 if there are none
		 * (or always with AI_ALL) */
		int have6 = 0, k = 0;
		for (int i = 0; i < naddrs; i++) if (addrs[i].family == AF_INET6) have6 = 1;
		for (int i = 0; i < naddrs; i++) {
			if (addrs[i].family == AF_INET6) {
				addrs[k++] = addrs[i];
			} else if (!have6 || (flags & AI_ALL)) {
				uint8_t v4[4];
				memcpy(v4, addrs[i].addr, 4);
				memset(addrs[k].addr, 0, 10);
				addrs[k].addr[10] = addrs[k].addr[11] = 0xff;
				memcpy(addrs[k].addr + 12, v4, 4);
				addrs[k].family = AF_INET6;
				addrs[k].scopeid = 0;
				k++;
			}
		}
		naddrs = k;
		if (!naddrs) return EAI_NONAME;
	}

	struct addrinfo *head = 0, **tail = &head;
	size_t clen = (flags & AI_CANONNAME) ? strlen(canon) + 1 : 0;
	for (int i = 0; i < naddrs; i++) {
		for (int j = 0; j < nservs; j++) {
			struct aibuf *b = calloc(1, sizeof *b + (head ? 0 : clen));
			if (!b) {
				freeaddrinfo(head);
				return EAI_MEMORY;
			}
			b->ai.ai_family = addrs[i].family;
			b->ai.ai_socktype = servs[j].socktype;
			b->ai.ai_protocol = servs[j].proto;
			b->ai.ai_addr = (void *)&b->sa;
			if (addrs[i].family == AF_INET) {
				b->ai.ai_addrlen = sizeof b->sa.sin;
				b->sa.sin.sin_family = AF_INET;
				b->sa.sin.sin_port = htons(servs[j].port);
				memcpy(&b->sa.sin.sin_addr, addrs[i].addr, 4);
			} else {
				b->ai.ai_addrlen = sizeof b->sa.sin6;
				b->sa.sin6.sin6_family = AF_INET6;
				b->sa.sin6.sin6_port = htons(servs[j].port);
				b->sa.sin6.sin6_scope_id = addrs[i].scopeid;
				memcpy(&b->sa.sin6.sin6_addr, addrs[i].addr, 16);
			}
			if (!head && clen) {
				memcpy(b->canon, canon, clen);
				b->ai.ai_canonname = b->canon;
			}
			*tail = &b->ai;
			tail = &b->ai.ai_next;
		}
	}
	*res = head;
	return 0;
}

void freeaddrinfo(struct addrinfo *p)
{
	while (p) {
		struct addrinfo *n = p->ai_next;
		free(p);
		p = n;
	}
}

const char *gai_strerror(int e)
{
	switch (e) {
	case 0: return "Success";
	case EAI_BADFLAGS: return "Invalid flags";
	case EAI_NONAME: return "Name does not resolve";
	case EAI_AGAIN: return "Temporary failure in name resolution";
	case EAI_FAIL: return "Non-recoverable failure in name resolution";
	case EAI_NODATA: return "No address associated with name";
	case EAI_FAMILY: return "Address family not supported";
	case EAI_SOCKTYPE: return "Socket type not supported";
	case EAI_SERVICE: return "Service not supported for socket type";
	case EAI_MEMORY: return "Out of memory";
	case EAI_SYSTEM: return "System error";
	case EAI_OVERFLOW: return "Argument buffer too small";
	case EAI_ADDRFAMILY: return "Address family for name not supported";
	}
	return "Unknown error";
}

static const char *serv_name(int port, const char *proto, char *buf, size_t len)
{
	FILE *f = fopen("/etc/services", "re");
	if (!f) return 0;
	char line[512];
	const char *r = 0;
	while (!r && fgets(line, sizeof line, f)) {
		char *p = strchr(line, '#');
		if (p) *p = 0;
		char *save, *n = strtok_r(line, " \t\n", &save), *pp;
		if (!n || !(pp = strtok_r(0, " \t\n", &save))) continue;
		char *slash = strchr(pp, '/');
		if (!slash || strcmp(slash + 1, proto)) continue;
		*slash = 0;
		if (atoi(pp) != port) continue;
		if (strlen(n) < len) {
			strcpy(buf, n);
			r = buf;
		}
	}
	fclose(f);
	return r;
}

int getnameinfo(const struct sockaddr *restrict sa, socklen_t sl, char *restrict host,
                socklen_t hostlen, char *restrict serv, socklen_t servlen, int flags)
{
	struct address a;
	int port;
	memset(&a, 0, sizeof a);
	if (sa->sa_family == AF_INET) {
		if (sl < sizeof(struct sockaddr_in)) return EAI_FAMILY;
		const struct sockaddr_in *s = (const void *)sa;
		a.family = AF_INET;
		memcpy(a.addr, &s->sin_addr, 4);
		port = ntohs(s->sin_port);
	} else if (sa->sa_family == AF_INET6) {
		if (sl < sizeof(struct sockaddr_in6)) return EAI_FAMILY;
		const struct sockaddr_in6 *s = (const void *)sa;
		a.family = AF_INET6;
		memcpy(a.addr, &s->sin6_addr, 16);
		a.scopeid = s->sin6_scope_id;
		port = ntohs(s->sin6_port);
	} else {
		return EAI_FAMILY;
	}

	if (host && hostlen) {
		int done = 0;
		if (!(flags & NI_NUMERICHOST)) {
			int r = __reverse_lookup(&a, host, hostlen, flags & NI_NAMEREQD);
			if (!r) {
				done = 1;
				if (flags & NI_NOFQDN) {
					char *dot = strchr(host, '.');
					if (dot) *dot = 0;
				}
			} else if (r == EAI_OVERFLOW) {
				return r;
			} else if (flags & NI_NAMEREQD) {
				return r == EAI_AGAIN ? EAI_AGAIN : EAI_NONAME;
			}
		}
		if (!done) {
			char buf[INET6_ADDRSTRLEN + IF_NAMESIZE + 2];
			inet_ntop(a.family, a.addr, buf, sizeof buf);
			if (a.family == AF_INET6 && a.scopeid) {
				char *p = buf + strlen(buf);
				*p++ = '%';
				const struct in6_addr *ia = (const void *)a.addr;
				if (!(flags & NI_NUMERICSCOPE) &&
				    (IN6_IS_ADDR_LINKLOCAL(ia) || IN6_IS_ADDR_MC_LINKLOCAL(ia)) &&
				    if_indextoname(a.scopeid, p)) {
					/* interface name */
				} else {
					sprintf(p, "%u", a.scopeid);
				}
			}
			if (strlen(buf) >= hostlen) return EAI_OVERFLOW;
			strcpy(host, buf);
		}
	}
	if (serv && servlen) {
		char buf[64];
		const char *n = 0;
		if (!(flags & NI_NUMERICSERV)) n = serv_name(port, flags & NI_DGRAM ? "udp" : "tcp", buf, sizeof buf);
		if (!n) {
			snprintf(buf, sizeof buf, "%d", port);
			n = buf;
		}
		if (strlen(n) >= servlen) return EAI_OVERFLOW;
		strcpy(serv, n);
	}
	return 0;
}
