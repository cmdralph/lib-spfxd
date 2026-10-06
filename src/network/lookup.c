/*
 * lib-spfxd — name and service lookup for getaddrinfo and friends.
 *
 * Name lookup order: numeric literal, "localhost" (and *.localhost),
 * /etc/hosts, then DNS with the resolv.conf search list.  When both IPv6
 * and IPv4 results exist they are ordered after RFC 6724: destinations
 * the system cannot route to (a connect() of a UDP socket fails) go last,
 * then by the default policy-table precedence.
 */
#include <ctype.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "netdb_impl.h"

int __lookup_ipliteral(struct address *buf, const char *name, int family)
{
	struct in_addr a4;
	struct in6_addr a6;
	if (inet_aton(name, &a4) > 0) {
		if (family == AF_INET6) return EAI_ADDRFAMILY;
		buf->family = AF_INET;
		buf->scopeid = 0;
		memcpy(buf->addr, &a4, 4);
		return 1;
	}
	char tmp[64];
	const char *p = strchr(name, '%');
	if (p) {
		if ((size_t)(p - name) >= sizeof tmp) return 0;
		memcpy(tmp, name, (size_t)(p - name));
		tmp[p - name] = 0;
		name = tmp;
	}
	if (inet_pton(AF_INET6, name, &a6) <= 0) return 0;
	if (family == AF_INET) return EAI_ADDRFAMILY;
	buf->family = AF_INET6;
	memcpy(buf->addr, &a6, 16);
	buf->scopeid = 0;
	if (p) {
		char *e;
		unsigned long id = strtoul(p + 1, &e, 10);
		if (*e || e == p + 1) {
			if (!IN6_IS_ADDR_LINKLOCAL(&a6) && !IN6_IS_ADDR_MC_LINKLOCAL(&a6)) return EAI_NONAME;
			id = if_nametoindex(p + 1);
			if (!id) return EAI_NONAME;
		}
		buf->scopeid = (unsigned)id;
	}
	return 1;
}

static int name_ok(const char *s)
{
	size_t l = strnlen(s, 255);
	if (!l || l > 253) return 0;
	for (; *s; s++)
		if ((unsigned char)*s < 0x20 || *s == ' ' || *s == '\x7f') return 0;
	return 1;
}

static int lookup_hosts(struct address *buf, char *canon, const char *name, int family)
{
	FILE *f = fopen("/etc/hosts", "re");
	if (!f) return 0;
	char line[512];
	int cnt = 0;
	while (fgets(line, sizeof line, f) && cnt < MAXADDRS) {
		char *p = strchr(line, '#');
		if (p) *p = 0;
		char *save, *addr = strtok_r(line, " \t\n", &save);
		if (!addr) continue;
		char *tok, *first = 0;
		int match = 0;
		while ((tok = strtok_r(0, " \t\n", &save))) {
			if (!first) first = tok;
			if (!strcasecmp(tok, name)) match = 1;
		}
		if (!match) continue;
		struct address a;
		if (__lookup_ipliteral(&a, addr, family) != 1) continue;
		buf[cnt++] = a;
		if (first && strlen(first) < 256 && !*canon) strcpy(canon, first);
	}
	fclose(f);
	return cnt;
}

struct dns_ctx {
	struct address *buf;
	int cnt;
	char *canon;
	int family;
};

static int dns_cb(void *c, int type, const unsigned char *data, int len, const unsigned char *pkt, int plen)
{
	struct dns_ctx *ctx = c;
	char tmp[256];
	if (type == 1 && len == 4 && ctx->cnt < MAXADDRS) {
		ctx->buf[ctx->cnt].family = AF_INET;
		ctx->buf[ctx->cnt].scopeid = 0;
		memcpy(ctx->buf[ctx->cnt++].addr, data, 4);
	} else if (type == 28 && len == 16 && ctx->cnt < MAXADDRS) {
		ctx->buf[ctx->cnt].family = AF_INET6;
		ctx->buf[ctx->cnt].scopeid = 0;
		memcpy(ctx->buf[ctx->cnt++].addr, data, 16);
	} else if (type == 5) {
		if (__dn_expand(pkt, pkt + plen, data, tmp, sizeof tmp) > 0 && name_ok(tmp))
			strcpy(ctx->canon, tmp);
	}
	return 0;
}

/* Query one fully qualified name: >0 addresses, 0 no such name / no
 * data, <0 an EAI_ error. */
static int name_from_dns(struct address *buf, char *canon, const char *name, int family,
                         const struct resolv_conf *conf)
{
	unsigned char qbuf[2][280], abuf[2][4800];
	const unsigned char *qp[2];
	unsigned char *ap[2] = { abuf[0], abuf[1] };
	int qlens[2], alens[2], nq = 0;
	static const struct { int af, rr; } afrr[2] = { { AF_INET6, 28 }, { AF_INET, 1 } };
	for (int i = 0; i < 2; i++) {
		if (family != afrr[i].af) {
			if (family != AF_UNSPEC) continue;
		}
		qlens[nq] = __dns_mkquery(name, afrr[i].rr, qbuf[nq], sizeof qbuf[nq]);
		if (qlens[nq] < 0) return EAI_NONAME;
		qbuf[nq][3] = 0;
		qp[nq] = qbuf[nq];
		nq++;
	}
	int r = __dns_send(qp, qlens, ap, alens, sizeof abuf[0], nq, conf);
	if (r) return r;
	struct dns_ctx ctx = { buf, 0, canon, family };
	int nx = 0;
	for (int i = 0; i < nq; i++) {
		if (alens[i] < 12) return EAI_AGAIN;
		int rcode = abuf[i][3] & 15;
		if (rcode == 3) { nx++; continue; }
		if (rcode) return rcode == 2 ? EAI_AGAIN : EAI_FAIL;
		__dns_parse(abuf[i], alens[i], dns_cb, &ctx);
	}
	if (ctx.cnt) {
		if (!*canon) strcpy(canon, name);
		return ctx.cnt;
	}
	(void)nx;
	return 0;
}

static int name_from_dns_search(struct address *buf, char *canon, const char *name, int family)
{
	struct resolv_conf conf;
	__get_resolv_conf(&conf);
	size_t l = strlen(name);
	int dots = 0;
	for (size_t i = 0; i < l; i++) if (name[i] == '.') dots++;
	int absolute = l && name[l - 1] == '.';
	char fq[512];
	int r;
	if (absolute || dots >= conf.ndots) {
		canon[0] = 0;
		r = name_from_dns(buf, canon, name, family, &conf);
		if (r || absolute) return r ? r : EAI_NONAME;
	}
	for (const char *p = conf.search, *z; *p; p = z) {
		while (*p == ' ' || *p == '\t') p++;
		for (z = p; *z && *z != ' ' && *z != '\t'; z++);
		if (z == p) break;
		size_t dl = (size_t)(z - p);
		if (l + 1 + dl + 1 > sizeof fq || l + 1 + dl > 253) continue;
		memcpy(fq, name, l);
		fq[l] = '.';
		memcpy(fq + l + 1, p, dl);
		fq[l + 1 + dl] = 0;
		canon[0] = 0;
		r = name_from_dns(buf, canon, fq, family, &conf);
		if (r) return r;
	}
	if (dots < conf.ndots) {
		canon[0] = 0;
		r = name_from_dns(buf, canon, name, family, &conf);
		if (r) return r;
	}
	return EAI_NONAME;
}

/* RFC 6724 default policy table precedence */
static int precedence(const struct address *a)
{
	if (a->family == AF_INET) return 35;
	const uint8_t *b = a->addr;
	static const uint8_t lo[16] = { [15] = 1 };
	if (!memcmp(b, lo, 16)) return 50;
	if (b[0] == 0x20 && b[1] == 0x02) return 30;
	if (b[0] == 0x20 && b[1] == 0x01 && !b[2] && !b[3]) return 5;
	if ((b[0] & 0xfe) == 0xfc) return 3;
	if (b[0] == 0xfe && (b[1] & 0xc0) == 0xc0) return 1;
	return 40;
}

static int reachable(const struct address *a)
{
	int fd = socket(a->family, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
	if (fd < 0) return 0;
	int ok;
	if (a->family == AF_INET) {
		struct sockaddr_in s = { .sin_family = AF_INET, .sin_port = htons(65535) };
		memcpy(&s.sin_addr, a->addr, 4);
		ok = connect(fd, (void *)&s, sizeof s) == 0;
	} else {
		struct sockaddr_in6 s = { .sin6_family = AF_INET6, .sin6_port = htons(65535),
		                          .sin6_scope_id = a->scopeid };
		memcpy(&s.sin6_addr, a->addr, 16);
		ok = connect(fd, (void *)&s, sizeof s) == 0;
	}
	close(fd);
	return ok;
}

static int addrcmp(const void *x, const void *y)
{
	const struct address *a = x, *b = y;
	return b->sortkey - a->sortkey;
}

int __lookup_name(struct address *buf, char *canon, const char *name, int family, int flags)
{
	*canon = 0;
	int cnt;
	if (strlen(name) < 256) strcpy(canon, name);
	cnt = __lookup_ipliteral(buf, name, family);
	if (cnt) return cnt;
	if (flags & AI_NUMERICHOST) return EAI_NONAME;
	if (!name_ok(name)) return EAI_NONAME;

	size_t l = strlen(name);
	if (!strcasecmp(name, "localhost") || !strcasecmp(name, "localhost.") ||
	    (l > 10 && !strcasecmp(name + l - 10, ".localhost"))) {
		cnt = 0;
		if (family != AF_INET) {
			memset(&buf[cnt], 0, sizeof buf[cnt]);
			buf[cnt].family = AF_INET6;
			buf[cnt++].addr[15] = 1;
		}
		if (family != AF_INET6) {
			memset(&buf[cnt], 0, sizeof buf[cnt]);
			buf[cnt].family = AF_INET;
			memcpy(buf[cnt++].addr, "\x7f\0\0\x01", 4);
		}
		return cnt;
	}
	*canon = 0;
	cnt = lookup_hosts(buf, canon, name, family);
	if (!cnt) {
		cnt = name_from_dns_search(buf, canon, name, family);
		if (cnt < 0) return cnt;
	}
	if (!*canon && strlen(name) < 256) strcpy(canon, name);

	/* RFC 6724 ordering when both families are present */
	int have4 = 0, have6 = 0;
	for (int i = 0; i < cnt; i++) {
		if (buf[i].family == AF_INET) have4 = 1;
		else have6 = 1;
	}
	if (have4 && have6) {
		for (int i = 0; i < cnt; i++)
			buf[i].sortkey = (reachable(&buf[i]) ? 1 << 20 : 0) + (precedence(&buf[i]) << 8) + (MAXADDRS - i);
		qsort(buf, (size_t)cnt, sizeof *buf, addrcmp);
	}
	return cnt;
}

static int serv_from_file(const char *name, const char *proto, uint16_t *port)
{
	FILE *f = fopen("/etc/services", "re");
	if (!f) return 0;
	char line[512];
	int found = 0;
	while (!found && fgets(line, sizeof line, f)) {
		char *p = strchr(line, '#');
		if (p) *p = 0;
		char *save, *n = strtok_r(line, " \t\n", &save);
		char *pp = n ? strtok_r(0, " \t\n", &save) : 0;
		if (!pp) continue;
		char *slash = strchr(pp, '/');
		if (!slash || strcmp(slash + 1, proto)) continue;
		int match = !strcmp(n, name);
		for (char *tok; !match && (tok = strtok_r(0, " \t\n", &save));)
			if (!strcmp(tok, name)) match = 1;
		if (!match) continue;
		*slash = 0;
		char *e;
		unsigned long v = strtoul(pp, &e, 10);
		if (*e || v > 65535) continue;
		*port = (uint16_t)v;
		found = 1;
	}
	fclose(f);
	return found;
}

int __lookup_serv(struct service *buf, const char *name, int proto, int socktype, int flags)
{
	int cnt = 0;
	switch (socktype) {
	case SOCK_STREAM:
		if (!proto) proto = IPPROTO_TCP;
		else if (proto != IPPROTO_TCP) return EAI_SERVICE;
		break;
	case SOCK_DGRAM:
		if (!proto) proto = IPPROTO_UDP;
		else if (proto != IPPROTO_UDP) return EAI_SERVICE;
		break;
	case 0:
		break;
	default:
		if (name) return EAI_SERVICE;
		buf[0] = (struct service){ 0, (unsigned char)proto, (unsigned char)socktype };
		return 1;
	}
	unsigned long port = 0;
	if (name) {
		if (!*name) return EAI_SERVICE;
		char *e;
		port = strtoul(name, &e, 10);
		if (!*e) {
			if (port > 65535) return EAI_SERVICE;
			name = 0;
		} else if (flags & AI_NUMERICSERV) {
			return EAI_NONAME;
		}
	}
	if (!name) {
		if (proto != IPPROTO_UDP) buf[cnt++] = (struct service){ (uint16_t)port, IPPROTO_TCP, SOCK_STREAM };
		if (proto != IPPROTO_TCP) buf[cnt++] = (struct service){ (uint16_t)port, IPPROTO_UDP, SOCK_DGRAM };
		return cnt;
	}
	uint16_t p;
	if (proto != IPPROTO_UDP && serv_from_file(name, "tcp", &p))
		buf[cnt++] = (struct service){ p, IPPROTO_TCP, SOCK_STREAM };
	if (proto != IPPROTO_TCP && serv_from_file(name, "udp", &p))
		buf[cnt++] = (struct service){ p, IPPROTO_UDP, SOCK_DGRAM };
	return cnt ? cnt : EAI_SERVICE;
}

static int ptr_cb(void *c, int type, const unsigned char *data, int len, const unsigned char *pkt, int plen)
{
	char *out = c;
	char tmp[256];
	(void)len;
	if (type == 12 && !*out && __dn_expand(pkt, pkt + plen, data, tmp, sizeof tmp) > 0 && name_ok(tmp))
		strcpy(out, tmp);
	return 0;
}

static int reverse_hosts(const struct address *a, char *out)
{
	FILE *f = fopen("/etc/hosts", "re");
	if (!f) return 0;
	char line[512];
	int found = 0;
	while (!found && fgets(line, sizeof line, f)) {
		char *p = strchr(line, '#');
		if (p) *p = 0;
		char *save, *addr = strtok_r(line, " \t\n", &save), *n;
		if (!addr || !(n = strtok_r(0, " \t\n", &save))) continue;
		struct address b;
		if (__lookup_ipliteral(&b, addr, AF_UNSPEC) != 1 || b.family != a->family) continue;
		if (memcmp(b.addr, a->addr, a->family == AF_INET ? 4 : 16)) continue;
		if (strlen(n) < 256) {
			strcpy(out, n);
			found = 1;
		}
	}
	fclose(f);
	return found;
}

int __reverse_lookup(const struct address *a, char *host, size_t hostlen, int namereqd)
{
	char name[256] = "", q[80];
	(void)namereqd;
	if (!reverse_hosts(a, name)) {
		char *p = q;
		if (a->family == AF_INET) {
			sprintf(q, "%d.%d.%d.%d.in-addr.arpa", a->addr[3], a->addr[2], a->addr[1], a->addr[0]);
		} else {
			for (int i = 15; i >= 0; i--)
				p += sprintf(p, "%x.%x.", a->addr[i] & 15, a->addr[i] >> 4);
			strcpy(p, "ip6.arpa");
		}
		struct resolv_conf conf;
		__get_resolv_conf(&conf);
		unsigned char qb[280], ab[4800], *ap = ab;
		const unsigned char *qp = qb;
		int ql = __dns_mkquery(q, 12, qb, sizeof qb), al;
		if (ql < 0) return EAI_NONAME;
		int r = __dns_send(&qp, &ql, &ap, &al, sizeof ab, 1, &conf);
		if (r) return r;
		if (al >= 12) {
			int rcode = ab[3] & 15;
			if (rcode == 2) return EAI_AGAIN;
			if (!rcode) __dns_parse(ab, al, ptr_cb, name);
		}
	}
	if (!*name) return EAI_NONAME;
	if (strlen(name) >= hostlen) return EAI_OVERFLOW;
	strcpy(host, name);
	return 0;
}
