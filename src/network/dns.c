/*
 * lib-spfxd — DNS stub resolver.
 *
 * Configuration comes from /etc/resolv.conf: up to three `nameserver`
 * lines (127.0.0.1 if none), `search`/`domain`, and `options ndots:N
 * timeout:N attempts:N`.  Queries for several record types are sent in
 * parallel to every configured server over UDP; the first usable answer
 * to each query wins.  A truncated answer is retried over TCP with the
 * server that sent it.  Retransmission happens every `timeout / attempts`
 * seconds until `timeout` expires.  Answers are matched by query id and
 * the question section, and must come from the address queried.
 */
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <time.h>
#include <unistd.h>
#include "netdb_impl.h"

int __get_resolv_conf(struct resolv_conf *c)
{
	memset(c, 0, sizeof *c);
	c->ndots = 1;
	c->timeout = 5;
	c->attempts = 2;
	FILE *f = fopen("/etc/resolv.conf", "re");
	if (f) {
		char line[512];
		while (fgets(line, sizeof line, f)) {
			char *p = line, *e;
			if (!strchr(line, '\n') && !feof(f)) {
				/* overlong line: skip the rest */
				int ch;
				while ((ch = getc(f)) != EOF && ch != '\n');
				continue;
			}
			if (!strncmp(p, "nameserver", 10) && (p[10] == ' ' || p[10] == '\t')) {
				p += 11;
				while (*p == ' ' || *p == '\t') p++;
				for (e = p; *e && *e != ' ' && *e != '\t' && *e != '\n'; e++);
				*e = 0;
				if (c->nns >= MAXNS) continue;
				struct address a[1];
				if (__lookup_ipliteral(a, p, AF_UNSPEC) == 1) {
					if (a->family == AF_INET) {
						struct sockaddr_in *s = (void *)&c->ns[c->nns];
						s->sin_family = AF_INET;
						s->sin_port = htons(53);
						memcpy(&s->sin_addr, a->addr, 4);
						c->nslen[c->nns++] = sizeof *s;
					} else {
						struct sockaddr_in6 *s = (void *)&c->ns[c->nns];
						s->sin6_family = AF_INET6;
						s->sin6_port = htons(53);
						s->sin6_scope_id = a->scopeid;
						memcpy(&s->sin6_addr, a->addr, 16);
						c->nslen[c->nns++] = sizeof *s;
					}
				}
			} else if ((!strncmp(p, "search", 6) || !strncmp(p, "domain", 6)) &&
			           (p[6] == ' ' || p[6] == '\t')) {
				p += 7;
				while (*p == ' ' || *p == '\t') p++;
				size_t l = strcspn(p, "\n");
				if (l >= sizeof c->search) l = sizeof c->search - 1;
				memcpy(c->search, p, l);
				c->search[l] = 0;
			} else if (!strncmp(p, "options", 7) && (p[7] == ' ' || p[7] == '\t')) {
				for (p += 8; *p; p++) {
					if (!strncmp(p, "ndots:", 6)) {
						long v = strtol(p + 6, &e, 10);
						if (e != p + 6) c->ndots = v > 15 ? 15 : (int)v;
					} else if (!strncmp(p, "attempts:", 9)) {
						long v = strtol(p + 9, &e, 10);
						if (e != p + 9) c->attempts = v > 10 ? 10 : v < 1 ? 1 : (int)v;
					} else if (!strncmp(p, "timeout:", 8)) {
						long v = strtol(p + 8, &e, 10);
						if (e != p + 8) c->timeout = v > 60 ? 60 : v < 1 ? 1 : (int)v;
					}
				}
			}
		}
		fclose(f);
	}
	if (!c->nns) {
		struct sockaddr_in *s = (void *)&c->ns[0];
		s->sin_family = AF_INET;
		s->sin_port = htons(53);
		s->sin_addr.s_addr = htonl(0x7f000001);
		c->nslen[0] = sizeof *s;
		c->nns = 1;
	}
	return 0;
}

/* Build a standard query (RD set) for name/type.  Returns its length. */
int __dns_mkquery(const char *name, int type, unsigned char *q, int bufsz)
{
	size_t l = strlen(name);
	if (l && name[l - 1] == '.') l--;
	if (l > 253 || bufsz < (int)l + 18) return -1;
	memset(q, 0, 12);
	uint16_t id;
	if (getrandom(&id, sizeof id, GRND_NONBLOCK) != sizeof id) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		id = (uint16_t)(ts.tv_nsec ^ (ts.tv_nsec >> 16) ^ (uintptr_t)q);
	}
	q[0] = (unsigned char)(id >> 8);
	q[1] = (unsigned char)id;
	q[2] = 1;                         /* RD */
	q[5] = 1;                         /* QDCOUNT */
	unsigned char *p = q + 12;
	const char *s = name, *end = name + l;
	if (!l) {
		*p++ = 0;
	} else {
		while (s < end) {
			const char *dot = memchr(s, '.', (size_t)(end - s));
			size_t ll = (size_t)((dot ? dot : end) - s);
			if (!ll || ll > 63) return -1;
			*p++ = (unsigned char)ll;
			memcpy(p, s, ll);
			p += ll;
			s += ll + (dot ? 1 : 0);
		}
		*p++ = 0;
	}
	*p++ = (unsigned char)(type >> 8);
	*p++ = (unsigned char)type;
	*p++ = 0;
	*p++ = 1;                         /* class IN */
	return (int)(p - q);
}

/* Expand a possibly compressed domain name at src.  Returns the number of
 * bytes the name occupies at src, or -1. */
int __dn_expand(const unsigned char *base, const unsigned char *end,
                const unsigned char *src, char *dest, int space)
{
	const unsigned char *p = src;
	char *d = dest, *dend = dest + (space > 254 ? 254 : space);
	int len = -1, hops = 0;
	if (p >= end || space < 1) return -1;
	for (;;) {
		if (p >= end) return -1;
		if ((*p & 0xc0) == 0xc0) {
			if (p + 1 >= end) return -1;
			if (len < 0) len = (int)(p + 2 - src);
			size_t off = (size_t)((p[0] & 0x3f) << 8 | p[1]);
			if (off >= (size_t)(end - base) || ++hops > 64) return -1;
			p = base + off;
			continue;
		}
		if (*p & 0xc0) return -1;
		size_t l = *p++;
		if (!l) break;
		if (p + l > end || d + l + 1 > dend) return -1;
		if (d != dest) *d++ = '.';
		for (size_t k = 0; k < l; k++) {
			unsigned char ch = p[k];
			*d++ = (char)(ch ? ch : '?');
		}
		p += l;
	}
	*d = 0;
	if (len < 0) len = (int)(p - src);
	return len;
}

/* Skip a name; returns pointer after it or NULL. */
static const unsigned char *skip_name(const unsigned char *p, const unsigned char *end)
{
	while (p < end) {
		if ((*p & 0xc0) == 0xc0) return p + 2 <= end ? p + 2 : 0;
		if (!*p) return p + 1;
		p += *p + 1;
	}
	return 0;
}

/* Call cb(ctx, type, rdata, rdlen, packet, plen) for each answer RR. */
int __dns_parse(const unsigned char *r, int rlen,
                int (*cb)(void *, int, const unsigned char *, int, const unsigned char *, int), void *ctx)
{
	const unsigned char *end = r + rlen, *p;
	if (rlen < 12) return -1;
	if (r[3] & 15) return 0;              /* rcode */
	int qd = r[4] << 8 | r[5], an = r[6] << 8 | r[7];
	p = r + 12;
	while (qd--) {
		p = skip_name(p, end);
		if (!p || p + 4 > end) return -1;
		p += 4;
	}
	while (an--) {
		p = skip_name(p, end);
		if (!p || p + 10 > end) return -1;
		int type = p[0] << 8 | p[1];
		int len = p[8] << 8 | p[9];
		p += 10;
		if (p + len > end) return -1;
		if (cb(ctx, type, p, len, r, rlen) < 0) return -1;
		p += len;
	}
	return 0;
}

static unsigned long mtime_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long)ts.tv_sec * 1000 + (unsigned long)ts.tv_nsec / 1000000;
}

static int same_addr(const struct sockaddr *a, const struct sockaddr_storage *b)
{
	if (a->sa_family != b->ss_family) return 0;
	if (a->sa_family == AF_INET) {
		const struct sockaddr_in *x = (const void *)a, *y = (const void *)b;
		return x->sin_port == y->sin_port && x->sin_addr.s_addr == y->sin_addr.s_addr;
	}
	const struct sockaddr_in6 *x = (const void *)a, *y = (const void *)b;
	return x->sin6_port == y->sin6_port && !memcmp(&x->sin6_addr, &y->sin6_addr, 16);
}

/* Does answer r (len n) answer query q (len ql)?  Same id and question. */
static int answers_query(const unsigned char *r, int n, const unsigned char *q, int ql)
{
	if (n < ql || r[0] != q[0] || r[1] != q[1] || !(r[2] & 0x80)) return 0;
	return !memcmp(r + 12, q + 12, (size_t)(ql - 12)) || (r[3] & 15);
}

static int tcp_query(const struct sockaddr *sa, socklen_t sl, const unsigned char *q, int ql,
                     unsigned char *ans, int asz, int timeout_ms)
{
	int fd = socket(sa->sa_family, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (fd < 0) return -1;
	unsigned long deadline = mtime_ms() + (unsigned long)timeout_ms;
	int r = connect(fd, sa, sl);
	struct pollfd pfd = { fd, POLLOUT, 0 };
	if (r < 0 && errno != EINPROGRESS) goto fail;
	unsigned char hdr[2] = { (unsigned char)(ql >> 8), (unsigned char)ql };
	unsigned char lenbuf[2];
	int got = 0, want = -1;
	size_t sent = 0;
	const unsigned char *parts[2] = { hdr, q };
	size_t plen[2] = { 2, (size_t)ql };
	while (1) {
		long left = (long)(deadline - mtime_ms());
		if (left <= 0) goto fail;
		pfd.events = sent < (size_t)ql + 2 ? POLLOUT : POLLIN;
		if (poll(&pfd, 1, (int)left) <= 0) goto fail;
		if (sent < (size_t)ql + 2) {
			size_t idx = sent < 2 ? 0 : 1, off = sent < 2 ? sent : sent - 2;
			ssize_t k = send(fd, parts[idx] + off, plen[idx] - off, MSG_NOSIGNAL);
			if (k < 0 && errno != EAGAIN) goto fail;
			if (k > 0) sent += (size_t)k;
			continue;
		}
		if (want < 0) {
			ssize_t k = recv(fd, lenbuf + got, (size_t)(2 - got), 0);
			if (k <= 0 && !(k < 0 && errno == EAGAIN)) goto fail;
			if (k > 0) got += (int)k;
			if (got == 2) {
				want = lenbuf[0] << 8 | lenbuf[1];
				got = 0;
				if (want > asz) want = asz;
			}
			continue;
		}
		ssize_t k = recv(fd, ans + got, (size_t)(want - got), 0);
		if (k <= 0 && !(k < 0 && errno == EAGAIN)) goto fail;
		if (k > 0) got += (int)k;
		if (got == want) break;
	}
	close(fd);
	return got;
fail:
	close(fd);
	return -1;
}

int __dns_send(const unsigned char *const *q, const int *qlens, unsigned char **answers,
               int *alens, int asz, int n, const struct resolv_conf *conf)
{
	int family = AF_INET;
	for (int i = 0; i < conf->nns; i++)
		if (conf->ns[i].ss_family == AF_INET6) family = AF_INET6;
	int fd = socket(family, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (fd < 0 && family == AF_INET6) {
		family = AF_INET;
		fd = socket(family, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	}
	if (fd < 0) return EAI_SYSTEM;
	if (family == AF_INET6) {
		int zero = 0;
		setsockopt(fd, IPPROTO_IPV6, 26 /* IPV6_V6ONLY */, &zero, sizeof zero);
	}
	/* server addresses in the socket's family (v4 mapped if needed) */
	struct sockaddr_storage ns[MAXNS];
	socklen_t nsl[MAXNS];
	int nns = 0;
	for (int i = 0; i < conf->nns; i++) {
		if (conf->ns[i].ss_family == family) {
			ns[nns] = conf->ns[i];
			nsl[nns++] = conf->nslen[i];
		} else if (family == AF_INET6) {
			const struct sockaddr_in *s4 = (const void *)&conf->ns[i];
			struct sockaddr_in6 *s6 = (void *)&ns[nns];
			memset(s6, 0, sizeof *s6);
			s6->sin6_family = AF_INET6;
			s6->sin6_port = s4->sin_port;
			s6->sin6_addr.s6_addr[10] = s6->sin6_addr.s6_addr[11] = 0xff;
			memcpy(s6->sin6_addr.s6_addr + 12, &s4->sin_addr, 4);
			nsl[nns++] = sizeof *s6;
		}
	}
	for (int i = 0; i < n; i++) alens[i] = 0;
	int total_ms = conf->timeout * 1000;
	int retry_ms = total_ms / conf->attempts;
	unsigned long t0 = mtime_ms(), last_send = 0;
	int first = 1, done = 0;
	while (done < n) {
		unsigned long now = mtime_ms();
		if (now - t0 >= (unsigned long)total_ms) break;
		if (first || now - last_send >= (unsigned long)retry_ms) {
			for (int i = 0; i < n; i++) {
				if (alens[i]) continue;
				for (int k = 0; k < nns; k++)
					sendto(fd, q[i], (size_t)qlens[i], MSG_NOSIGNAL, (void *)&ns[k], nsl[k]);
			}
			last_send = now;
			first = 0;
		}
		unsigned long next = last_send + (unsigned long)retry_ms;
		long wait = (long)(next - now);
		long left = (long)(t0 + (unsigned long)total_ms - now);
		if (wait > left) wait = left;
		if (wait < 1) wait = 1;
		struct pollfd pfd = { fd, POLLIN, 0 };
		if (poll(&pfd, 1, (int)wait) <= 0) continue;
		for (;;) {
			struct sockaddr_storage from;
			socklen_t fl = sizeof from;
			unsigned char buf[4096];
			ssize_t r = recvfrom(fd, buf, sizeof buf, 0, (void *)&from, &fl);
			if (r < 0) break;
			int k;
			for (k = 0; k < nns && !same_addr((void *)&from, &ns[k]); k++);
			if (k == nns) continue;                /* not from a server we asked */
			for (int i = 0; i < n; i++) {
				if (alens[i] || !answers_query(buf, (int)r, q[i], qlens[i])) continue;
				if (buf[2] & 2) {
					/* truncated: retry over TCP with this server */
					int tl = tcp_query((void *)&ns[k], nsl[k], q[i], qlens[i], answers[i], asz,
					                   total_ms - (int)(mtime_ms() - t0));
					if (tl > 0 && answers_query(answers[i], tl, q[i], qlens[i])) {
						alens[i] = tl;
						done++;
					}
					continue;
				}
				int cl = (int)r < asz ? (int)r : asz;
				memcpy(answers[i], buf, (size_t)cl);
				alens[i] = cl;
				done++;
			}
		}
	}
	close(fd);
	return done ? 0 : EAI_AGAIN;
}
