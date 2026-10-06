/*
 * lib-spfxd — the services, protocols and networks databases
 * (/etc/services, /etc/protocols, /etc/networks).
 *
 * The non-reentrant functions return pointers into static storage that
 * the next call overwrites.  When /etc/protocols does not exist, a
 * built-in table of the common protocols is used.
 */
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netdb_impl.h"

#define MAXALIAS 16

/* Split a database line into tokens (comments removed); returns count. */
static int tokenize(char *line, char **tok, int max)
{
	char *p = strchr(line, '#'), *save;
	if (p) *p = 0;
	int n = 0;
	for (char *t = strtok_r(line, " \t\r\n", &save); t && n < max; t = strtok_r(0, " \t\r\n", &save))
		tok[n++] = t;
	return n;
}

/* ---------------------------------------------------------- services */

static FILE *serv_f;
static int serv_stay;

void setservent(int stay)
{
	if (serv_f) rewind(serv_f);
	serv_stay = stay;
}

void endservent(void)
{
	if (serv_f) fclose(serv_f);
	serv_f = 0;
}

/* Parse the next service entry from f into se (strings in buf). */
static int next_serv(FILE *f, struct servent *se, char *buf, size_t len, char **aliases)
{
	char *tok[MAXALIAS + 2];
	while (fgets(buf, (int)len, f)) {
		int n = tokenize(buf, tok, MAXALIAS + 2);
		if (n < 2) continue;
		char *slash = strchr(tok[1], '/');
		if (!slash) continue;
		*slash = 0;
		char *e;
		unsigned long port = strtoul(tok[1], &e, 10);
		if (*e || port > 65535) continue;
		se->s_name = tok[0];
		se->s_port = htons((uint16_t)port);
		se->s_proto = slash + 1;
		int k;
		for (k = 0; k + 2 < n && k < MAXALIAS - 1; k++) aliases[k] = tok[k + 2];
		aliases[k] = 0;
		se->s_aliases = aliases;
		return 1;
	}
	return 0;
}

struct servent *getservent(void)
{
	static struct servent se;
	static char buf[512];
	static char *aliases[MAXALIAS];
	if (!serv_f && !(serv_f = fopen("/etc/services", "re"))) return 0;
	if (next_serv(serv_f, &se, buf, sizeof buf, aliases)) return &se;
	if (!serv_stay) endservent();
	return 0;
}

static int serv_search(const char *name, int port, const char *proto, struct servent *se,
                       char *buf, size_t len, struct servent **res)
{
	*res = 0;
	size_t need = MAXALIAS * sizeof(char *) + 512;
	if (len < need) return ERANGE;
	char **aliases = (char **)(((uintptr_t)buf + 7) & ~(uintptr_t)7);
	char *line = (char *)(aliases + MAXALIAS);
	FILE *f = fopen("/etc/services", "re");
	if (!f) return errno;
	while (next_serv(f, se, line, 512, aliases)) {
		if (proto && strcmp(proto, se->s_proto)) continue;
		if (name) {
			int m = !strcmp(se->s_name, name);
			for (char **a = se->s_aliases; !m && *a; a++) m = !strcmp(*a, name);
			if (!m) continue;
		} else if (se->s_port != port) {
			continue;
		}
		*res = se;
		break;
	}
	fclose(f);
	return 0;
}

int getservbyname_r(const char *name, const char *proto, struct servent *se, char *buf, size_t len,
                    struct servent **res)
{
	return serv_search(name, 0, proto, se, buf, len, res);
}

int getservbyport_r(int port, const char *proto, struct servent *se, char *buf, size_t len,
                    struct servent **res)
{
	return serv_search(0, port, proto, se, buf, len, res);
}

static struct servent serv_static;
static char serv_buf[MAXALIAS * sizeof(char *) + 512 + 8];

struct servent *getservbyname(const char *name, const char *proto)
{
	struct servent *r;
	getservbyname_r(name, proto, &serv_static, serv_buf, sizeof serv_buf, &r);
	return r;
}

struct servent *getservbyport(int port, const char *proto)
{
	struct servent *r;
	getservbyport_r(port, proto, &serv_static, serv_buf, sizeof serv_buf, &r);
	return r;
}

/* --------------------------------------------------------- protocols */

static const struct { int num; const char *name, *alias; } builtin_protos[] = {
	{ 0, "ip", "IP" }, { 1, "icmp", "ICMP" }, { 2, "igmp", "IGMP" }, { 4, "ipencap", "IP-ENCAP" },
	{ 6, "tcp", "TCP" }, { 17, "udp", "UDP" }, { 41, "ipv6", "IPv6" }, { 47, "gre", "GRE" },
	{ 50, "esp", "IPSEC-ESP" }, { 51, "ah", "IPSEC-AH" }, { 58, "ipv6-icmp", "IPv6-ICMP" },
	{ 89, "ospf", "OSPFIGP" }, { 103, "pim", "PIM" }, { 132, "sctp", "SCTP" },
	{ 136, "udplite", "UDPLite" }, { 255, "raw", "RAW" },
};

static FILE *proto_f;
static int proto_stay, proto_idx, proto_builtin;

void setprotoent(int stay)
{
	if (proto_f) rewind(proto_f);
	proto_idx = 0;
	proto_stay = stay;
}

void endprotoent(void)
{
	if (proto_f) fclose(proto_f);
	proto_f = 0;
	proto_idx = 0;
	proto_builtin = 0;
}

struct protoent *getprotoent(void)
{
	static struct protoent pe;
	static char buf[512];
	static char *aliases[MAXALIAS];
	if (!proto_f && !proto_builtin) {
		proto_f = fopen("/etc/protocols", "re");
		if (!proto_f) proto_builtin = 1;
	}
	if (proto_builtin) {
		if ((size_t)proto_idx >= ARRAY_SIZE(builtin_protos)) return 0;
		pe.p_name = (char *)builtin_protos[proto_idx].name;
		pe.p_proto = builtin_protos[proto_idx].num;
		aliases[0] = (char *)builtin_protos[proto_idx].alias;
		aliases[1] = 0;
		pe.p_aliases = aliases;
		proto_idx++;
		return &pe;
	}
	char *tok[MAXALIAS + 2];
	while (fgets(buf, sizeof buf, proto_f)) {
		int n = tokenize(buf, tok, MAXALIAS + 2);
		if (n < 2) continue;
		pe.p_name = tok[0];
		pe.p_proto = atoi(tok[1]);
		int k;
		for (k = 0; k + 2 < n && k < MAXALIAS - 1; k++) aliases[k] = tok[k + 2];
		aliases[k] = 0;
		pe.p_aliases = aliases;
		return &pe;
	}
	if (!proto_stay) endprotoent();
	return 0;
}

struct protoent *getprotobyname(const char *name)
{
	struct protoent *p;
	setprotoent(0);
	while ((p = getprotoent())) {
		if (!strcmp(p->p_name, name)) break;
		int m = 0;
		for (char **a = p->p_aliases; !m && *a; a++) m = !strcmp(*a, name);
		if (m) break;
	}
	endprotoent();
	return p;
}

struct protoent *getprotobynumber(int num)
{
	struct protoent *p;
	setprotoent(0);
	while ((p = getprotoent()) && p->p_proto != num);
	endprotoent();
	return p;
}

/* ---------------------------------------------------------- networks */

static FILE *net_f;
static int net_stay;

void setnetent(int stay)
{
	if (net_f) rewind(net_f);
	net_stay = stay;
}

void endnetent(void)
{
	if (net_f) fclose(net_f);
	net_f = 0;
}

struct netent *getnetent(void)
{
	static struct netent ne;
	static char buf[512];
	static char *aliases[MAXALIAS];
	char *tok[MAXALIAS + 2];
	if (!net_f && !(net_f = fopen("/etc/networks", "re"))) return 0;
	while (fgets(buf, sizeof buf, net_f)) {
		int n = tokenize(buf, tok, MAXALIAS + 2);
		if (n < 2) continue;
		in_addr_t a = inet_network(tok[1]);
		if (a == (in_addr_t)-1) continue;
		ne.n_name = tok[0];
		ne.n_net = a;
		ne.n_addrtype = AF_INET;
		int k;
		for (k = 0; k + 2 < n && k < MAXALIAS - 1; k++) aliases[k] = tok[k + 2];
		aliases[k] = 0;
		ne.n_aliases = aliases;
		return &ne;
	}
	if (!net_stay) endnetent();
	return 0;
}

struct netent *getnetbyname(const char *name)
{
	struct netent *p;
	setnetent(0);
	while ((p = getnetent())) {
		if (!strcmp(p->n_name, name)) break;
		int m = 0;
		for (char **a = p->n_aliases; !m && *a; a++) m = !strcmp(*a, name);
		if (m) break;
	}
	endnetent();
	return p;
}

struct netent *getnetbyaddr(uint32_t net, int type)
{
	struct netent *p;
	setnetent(0);
	while ((p = getnetent()) && !(p->n_net == net && p->n_addrtype == type));
	endnetent();
	return p;
}
