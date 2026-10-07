/* lib-spfxd — byte order and IPv4/IPv6 address text conversion. */
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;

uint32_t htonl(uint32_t x) { return __builtin_bswap32(x); }
uint16_t htons(uint16_t x) { return __builtin_bswap16(x); }
uint32_t ntohl(uint32_t x) { return __builtin_bswap32(x); }
uint16_t ntohs(uint16_t x) { return __builtin_bswap16(x); }

/* inet_aton accepts the classic forms a, a.b, a.b.c, a.b.c.d with each
 * part in decimal, octal (0) or hex (0x). */
int inet_aton(const char *s, struct in_addr *dst)
{
	unsigned long parts[4];
	int n = 0;
	for (;;) {
		char *end;
		if (!isdigit((unsigned char)*s)) return 0;
		parts[n] = strtoul(s, &end, 0);
		if (end == s) return 0;
		s = end;
		n++;
		if (*s != '.' || n == 4) break;
		s++;
	}
	if (*s) return 0;
	unsigned long v;
	switch (n) {
	case 1:
		if (parts[0] > 0xffffffffUL) return 0;
		v = parts[0];
		break;
	case 2:
		if (parts[0] > 255 || parts[1] > 0xffffff) return 0;
		v = parts[0] << 24 | parts[1];
		break;
	case 3:
		if (parts[0] > 255 || parts[1] > 255 || parts[2] > 0xffff) return 0;
		v = parts[0] << 24 | parts[1] << 16 | parts[2];
		break;
	default:
		for (int i = 0; i < 4; i++)
			if (parts[i] > 255) return 0;
		v = parts[0] << 24 | parts[1] << 16 | parts[2] << 8 | parts[3];
	}
	if (dst) dst->s_addr = htonl((uint32_t)v);
	return 1;
}

in_addr_t inet_addr(const char *s)
{
	struct in_addr a;
	return inet_aton(s, &a) ? a.s_addr : INADDR_NONE;
}

in_addr_t inet_network(const char *s)
{
	struct in_addr a;
	return inet_aton(s, &a) ? ntohl(a.s_addr) : INADDR_NONE;
}

char *inet_ntoa(struct in_addr in)
{
	static char buf[INET_ADDRSTRLEN];
	unsigned char *a = (unsigned char *)&in;
	snprintf(buf, sizeof buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
	return buf;
}

struct in_addr inet_makeaddr(in_addr_t net, in_addr_t host)
{
	in_addr_t v;
	if (net < 256) v = net << 24 | (host & 0xffffff);
	else if (net < 65536) v = net << 16 | (host & 0xffff);
	else v = net << 8 | (host & 0xff);
	return (struct in_addr){ htonl(v) };
}

in_addr_t inet_lnaof(struct in_addr in)
{
	uint32_t h = ntohl(in.s_addr);
	if (h >> 31 == 0) return h & 0xffffff;
	if (h >> 30 == 2) return h & 0xffff;
	return h & 0xff;
}

in_addr_t inet_netof(struct in_addr in)
{
	uint32_t h = ntohl(in.s_addr);
	if (h >> 31 == 0) return h >> 24;
	if (h >> 30 == 2) return h >> 16;
	return h >> 8;
}

/* strict dotted quad for inet_pton: exactly four decimal parts, no
 * leading zeros */
static int pton4(const char *s, unsigned char *a)
{
	for (int i = 0; i < 4; i++) {
		int v = 0, d = 0;
		if (s[0] == '0' && isdigit((unsigned char)s[1])) return 0;
		for (; isdigit((unsigned char)*s) && d < 3; s++, d++) v = v * 10 + (*s - '0');
		if (!d || v > 255) return 0;
		a[i] = (unsigned char)v;
		if (i < 3 && *s++ != '.') return 0;
	}
	return !*s;
}

static int hexval(int c)
{
	if (c - '0' < 10u) return c - '0';
	c |= 32;
	if (c - 'a' < 6u) return c - 'a' + 10;
	return -1;
}

static int pton6(const char *s, unsigned char *a)
{
	uint16_t w[8];
	int n = 0, gap = -1;
	if (s[0] == ':') {
		if (s[1] != ':') return 0;
		s++;
	}
	for (;;) {
		if (*s == ':') {
			if (gap >= 0) return 0;
			gap = n;
			s++;
			if (!*s) break;
			continue;
		}
		if (n == 8) return 0;
		/* embedded IPv4 in the last 32 bits */
		const char *dot = strchr(s, '.');
		const char *colon = strchr(s, ':');
		if (dot && (!colon || dot < colon)) {
			if (n > 6) return 0;
			unsigned char v4[4];
			if (!pton4(s, v4)) return 0;
			w[n++] = (uint16_t)(v4[0] << 8 | v4[1]);
			w[n++] = (uint16_t)(v4[2] << 8 | v4[3]);
			s += strlen(s);
			break;
		}
		int v = 0, d = 0, h;
		for (; d < 4 && (h = hexval((unsigned char)*s)) >= 0; d++, s++) v = v * 16 + h;
		if (!d) return 0;
		w[n++] = (uint16_t)v;
		if (!*s) break;
		if (*s != ':') return 0;
		s++;
		if (!*s) return 0;
	}
	if (*s) return 0;
	if (gap < 0 && n != 8) return 0;
	if (gap >= 0 && n == 8) return 0;
	int fill = 8 - n;
	uint16_t out[8];
	for (int i = 0, k = 0; i < 8; i++) {
		if (gap >= 0 && i >= gap && i < gap + fill) out[i] = 0;
		else out[i] = w[k++];
	}
	for (int i = 0; i < 8; i++) {
		a[2 * i] = (unsigned char)(out[i] >> 8);
		a[2 * i + 1] = (unsigned char)out[i];
	}
	return 1;
}

int inet_pton(int af, const char *restrict s, void *restrict dst)
{
	if (af == AF_INET) return pton4(s, dst);
	if (af == AF_INET6) return pton6(s, dst);
	errno = EAFNOSUPPORT;
	return -1;
}

const char *inet_ntop(int af, const void *restrict src, char *restrict dst, socklen_t size)
{
	const unsigned char *a = src;
	char buf[INET6_ADDRSTRLEN];
	if (af == AF_INET) {
		if ((socklen_t)snprintf(buf, sizeof buf, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]) >= size) goto nospace;
	} else if (af == AF_INET6) {
		uint16_t w[8];
		for (int i = 0; i < 8; i++) w[i] = (uint16_t)(a[2 * i] << 8 | a[2 * i + 1]);
		/* RFC 5952: compress the longest run (>= 2) of zero groups */
		int best = -1, bestlen = 1;
		for (int i = 0; i < 8; ) {
			if (w[i]) { i++; continue; }
			int j = i;
			while (j < 8 && !w[j]) j++;
			if (j - i > bestlen) { best = i; bestlen = j - i; }
			i = j;
		}
		int p = 0;
		if (!memcmp(a, "\0\0\0\0\0\0\0\0\0\0\xff\xff", 12)) {
			p = snprintf(buf, sizeof buf, "::ffff:%d.%d.%d.%d", a[12], a[13], a[14], a[15]);
		} else {
			for (int i = 0; i < 8; i++) {
				if (i == best) {
					/* the previous group already printed its ':' */
					p += snprintf(buf + p, sizeof buf - (size_t)p, i ? ":" : "::");
					i += bestlen - 1;
					continue;
				}
				p += snprintf(buf + p, sizeof buf - (size_t)p, "%x%s", w[i], i < 7 ? ":" : "");
			}
		}
		if ((socklen_t)p >= size) goto nospace;
	} else {
		errno = EAFNOSUPPORT;
		return 0;
	}
	strcpy(dst, buf);
	return dst;
nospace:
	errno = ENOSPC;
	return 0;
}
