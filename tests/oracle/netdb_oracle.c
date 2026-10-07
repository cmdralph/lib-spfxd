/*
 * lib-spfxd oracle test — getaddrinfo/getnameinfo/services/protocols on
 * inputs that need no network access.  Result lists are printed sorted
 * (ordering between address families is a policy choice).
 */
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

static int cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static void gai(const char *host, const char *serv, int family, int socktype, int flags)
{
	struct addrinfo h, *res, *p;
	memset(&h, 0, sizeof h);
	h.ai_family = family;
	h.ai_socktype = socktype;
	h.ai_flags = flags;
	int r = getaddrinfo(host, serv, &h, &res);
	printf("gai(%s,%s,%d,%d,%#x) = %d", host ? host : "NULL", serv ? serv : "NULL", family, socktype, flags, r);
	if (r) { printf("\n"); return; }
	char *lines[64];
	int n = 0;
	for (p = res; p && n < 64; p = p->ai_next) {
		char a[64], line[160];
		if (p->ai_socktype == SOCK_RAW) continue;   /* glibc-only extra entries */
		if (p->ai_family == AF_INET) inet_ntop(AF_INET, &((struct sockaddr_in *)p->ai_addr)->sin_addr, a, sizeof a);
		else inet_ntop(AF_INET6, &((struct sockaddr_in6 *)p->ai_addr)->sin6_addr, a, sizeof a);
		int port = ntohs(((struct sockaddr_in *)p->ai_addr)->sin_port);
		snprintf(line, sizeof line, "%d/%d/%d %s:%d len=%u%s%s", p->ai_family, p->ai_socktype, p->ai_protocol,
		         a, port, (unsigned)p->ai_addrlen, p->ai_canonname ? " canon=" : "", p->ai_canonname ? p->ai_canonname : "");
		lines[n++] = strdup(line);
	}
	qsort(lines, (size_t)n, sizeof *lines, cmp);
	for (int i = 0; i < n; i++) { printf("\n  %s", lines[i]); free(lines[i]); }
	printf("\n");
	freeaddrinfo(res);
}

static void gni(const char *addr, int port, int flags)
{
	struct sockaddr_storage ss;
	memset(&ss, 0, sizeof ss);
	socklen_t sl;
	if (strchr(addr, ':')) {
		struct sockaddr_in6 *s = (void *)&ss;
		s->sin6_family = AF_INET6;
		s->sin6_port = htons((unsigned short)port);
		inet_pton(AF_INET6, addr, &s->sin6_addr);
		sl = sizeof *s;
	} else {
		struct sockaddr_in *s = (void *)&ss;
		s->sin_family = AF_INET;
		s->sin_port = htons((unsigned short)port);
		inet_pton(AF_INET, addr, &s->sin_addr);
		sl = sizeof *s;
	}
	char h[NI_MAXHOST], sv[NI_MAXSERV];
	int r = getnameinfo((void *)&ss, sl, h, sizeof h, sv, sizeof sv, flags);
	printf("gni(%s,%d,%#x) = %d", addr, port, flags, r);
	if (!r) printf(" %s %s", h, sv);
	printf("\n");
}

int main(void)
{
	gai("127.0.0.1", "80", AF_UNSPEC, 0, 0);
	gai("127.0.0.1", "http", AF_INET, SOCK_STREAM, 0);
	gai("::1", "443", AF_UNSPEC, SOCK_DGRAM, 0);
	gai("1.2.3.4", NULL, AF_INET, 0, AI_NUMERICHOST);
	gai("fe80::1", "22", AF_INET6, SOCK_STREAM, AI_NUMERICHOST);
	gai("1.2.3.4", NULL, AF_INET6, 0, AI_NUMERICHOST);
	gai("1.2.3.4", NULL, AF_INET6, 0, AI_NUMERICHOST | AI_V4MAPPED);
	gai("not a host", NULL, AF_UNSPEC, 0, AI_NUMERICHOST);
	gai(NULL, "8080", AF_INET, SOCK_STREAM, AI_PASSIVE);
	gai(NULL, "8080", AF_INET6, SOCK_STREAM, AI_PASSIVE);
	gai(NULL, "8080", AF_INET, SOCK_STREAM, 0);
	gai(NULL, NULL, AF_UNSPEC, 0, 0);
	gai("127.0.0.1", "nosuchservice", AF_UNSPEC, 0, 0);
	gai("127.0.0.1", "domain", AF_INET, 0, 0);
	gai("127.0.0.1", "http", AF_INET, 0, AI_NUMERICSERV);
	gai("localhost", "25", AF_INET, SOCK_STREAM, AI_CANONNAME);
	gai("127.0.0.1", NULL, AF_INET, SOCK_STREAM, AI_CANONNAME);
	gai("127.1", NULL, AF_INET, SOCK_STREAM, 0);
	gai("127.0.0.1", NULL, 12345, 0, 0);
	gai("127.0.0.1", NULL, AF_INET, 12345, 0);
	gai("127.0.0.1", NULL, AF_INET, 0, 0x4000);
	gni("127.0.0.1", 80, NI_NUMERICHOST | NI_NUMERICSERV);
	gni("127.0.0.1", 80, NI_NUMERICHOST);
	gni("127.0.0.1", 53, NI_NUMERICHOST | NI_DGRAM);
	gni("::1", 22, NI_NUMERICHOST);
	gni("10.9.8.7", 1, NI_NUMERICHOST | NI_NUMERICSERV);
	gni("127.0.0.1", 80, 0);
	struct servent *se = getservbyname("ssh", "tcp");
	printf("ssh/tcp %d %s\n", se ? ntohs(se->s_port) : -1, se ? se->s_proto : "-");
	se = getservbyport(htons(53), "udp");
	printf("53/udp %s\n", se ? se->s_name : "-");
	se = getservbyname("www", NULL);
	printf("www %d\n", se ? ntohs(se->s_port) : -1);
	struct protoent *pe = getprotobyname("udp");
	printf("udp %d\n", pe ? pe->p_proto : -1);
	pe = getprotobynumber(6);
	printf("6 %s\n", pe ? pe->p_name : "-");
	struct hostent *he = gethostbyname("127.0.0.1");
	if (he) printf("ghbn %s %d %d %d.%d.%d.%d\n", he->h_name, he->h_addrtype, he->h_length,
	               (unsigned char)he->h_addr[0], (unsigned char)he->h_addr[1], (unsigned char)he->h_addr[2], (unsigned char)he->h_addr[3]);
	he = gethostbyname("localhost");
	printf("ghbn localhost %s\n", he ? he->h_name : "-");
	return 0;
}
