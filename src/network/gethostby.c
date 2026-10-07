/*
 * lib-spfxd — gethostbyname/gethostbyname2/gethostbyaddr (and _r forms),
 * the hosts-file enumeration functions and h_errno.
 *
 * The reentrant forms lay the result out in the caller's buffer:
 * address list pointers, the addresses, an empty alias list and the
 * canonical name.  The non-reentrant forms use a static buffer.
 */
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netdb_impl.h"
#include "pthread_impl.h"

int *__h_errno_location(void)
{
	return &__self()->h_errno_val;
}

const char *hstrerror(int e)
{
	switch (e) {
	case 0: return "Resolver Error 0 (no error)";
	case HOST_NOT_FOUND: return "Unknown host";
	case TRY_AGAIN: return "Host name lookup failure";
	case NO_RECOVERY: return "Unknown server error";
	case NO_DATA: return "No address associated with name";
	}
	return "Unknown resolver error";
}

void herror(const char *msg)
{
	if (msg && *msg) fprintf(stderr, "%s: %s\n", msg, hstrerror(h_errno));
	else fprintf(stderr, "%s\n", hstrerror(h_errno));
}

static int eai_to_herr(int e)
{
	switch (e) {
	case EAI_AGAIN: return TRY_AGAIN;
	case EAI_FAIL: return NO_RECOVERY;
	case EAI_NODATA: return NO_DATA;
	default: return HOST_NOT_FOUND;
	}
}

/* Fill h in buf with n addresses of family af and the name. */
static int fill_hostent(struct hostent *h, char *buf, size_t buflen, int af,
                        const uint8_t (*addrs)[16], int n, const char *name)
{
	size_t alen = af == AF_INET ? 4 : 16;
	size_t nl = strlen(name) + 1;
	size_t need = (size_t)(n + 1) * sizeof(char *) + sizeof(char *) + (size_t)n * alen + nl + 16;
	if (buflen < need) return ERANGE;
	uintptr_t p = ((uintptr_t)buf + 7) & ~(uintptr_t)7;
	char **list = (char **)p;
	char **aliases = list + n + 1;
	char *data = (char *)(aliases + 1);
	for (int i = 0; i < n; i++) {
		memcpy(data, addrs[i], alen);
		list[i] = data;
		data += alen;
	}
	list[n] = 0;
	aliases[0] = 0;
	memcpy(data, name, nl);
	h->h_name = data;
	h->h_aliases = aliases;
	h->h_addrtype = af;
	h->h_length = (int)alen;
	h->h_addr_list = list;
	return 0;
}

int gethostbyname2_r(const char *name, int af, struct hostent *h, char *buf, size_t buflen,
                     struct hostent **res, int *err)
{
	struct address addrs[MAXADDRS];
	uint8_t raw[MAXADDRS][16];
	char canon[256];
	*res = 0;
	if (af != AF_INET && af != AF_INET6) {
		*err = NO_RECOVERY;
		return EAFNOSUPPORT;
	}
	int n = __lookup_name(addrs, canon, name, af, 0);
	if (n <= 0) {
		*err = n ? eai_to_herr(n) : HOST_NOT_FOUND;
		return n == EAI_AGAIN ? EAGAIN : (n == EAI_SYSTEM ? errno : 0);
	}
	int k = 0;
	for (int i = 0; i < n; i++)
		if (addrs[i].family == af) memcpy(raw[k++], addrs[i].addr, 16);
	if (!k) {
		*err = NO_DATA;
		return 0;
	}
	int r = fill_hostent(h, buf, buflen, af, (const uint8_t (*)[16])raw, k, canon);
	if (r) {
		*err = (-1);
		return r;
	}
	*res = h;
	*err = 0;
	return 0;
}

int gethostbyname_r(const char *name, struct hostent *h, char *buf, size_t buflen,
                    struct hostent **res, int *err)
{
	return gethostbyname2_r(name, AF_INET, h, buf, buflen, res, err);
}

int gethostbyaddr_r(const void *a, socklen_t l, int af, struct hostent *h, char *buf,
                    size_t buflen, struct hostent **res, int *err)
{
	struct address ad;
	char name[256];
	*res = 0;
	if ((af == AF_INET && l != 4) || (af == AF_INET6 && l != 16) || (af != AF_INET && af != AF_INET6)) {
		*err = NO_RECOVERY;
		return EINVAL;
	}
	memset(&ad, 0, sizeof ad);
	ad.family = af;
	memcpy(ad.addr, a, l);
	int r = __reverse_lookup(&ad, name, sizeof name, 1);
	if (r) {
		*err = eai_to_herr(r);
		return r == EAI_AGAIN ? EAGAIN : 0;
	}
	uint8_t raw[1][16];
	memcpy(raw[0], a, l);
	r = fill_hostent(h, buf, buflen, af, (const uint8_t (*)[16])raw, 1, name);
	if (r) {
		*err = (-1);
		return r;
	}
	*res = h;
	*err = 0;
	return 0;
}

static struct hostent static_h;
static char static_buf[MAXADDRS * 24 + 512];

struct hostent *gethostbyname2(const char *name, int af)
{
	struct hostent *r;
	int e = gethostbyname2_r(name, af, &static_h, static_buf, sizeof static_buf, &r, &h_errno);
	if (e && !r) errno = e;
	return r;
}

struct hostent *gethostbyname(const char *name)
{
	return gethostbyname2(name, AF_INET);
}

struct hostent *gethostbyaddr(const void *a, socklen_t l, int af)
{
	struct hostent *r;
	int e = gethostbyaddr_r(a, l, af, &static_h, static_buf, sizeof static_buf, &r, &h_errno);
	if (e && !r) errno = e;
	return r;
}

/* ---- /etc/hosts enumeration ---- */

static FILE *hosts_f;
static int hosts_stay;

void sethostent(int stay)
{
	if (hosts_f) rewind(hosts_f);
	hosts_stay = stay;
}

void endhostent(void)
{
	if (hosts_f) fclose(hosts_f);
	hosts_f = 0;
}

struct hostent *gethostent(void)
{
	static char line[512];
	static char *aliases[16];
	static char *list[2];
	static uint8_t addr[16];
	if (!hosts_f && !(hosts_f = fopen("/etc/hosts", "re"))) return 0;
	while (fgets(line, sizeof line, hosts_f)) {
		char *p = strchr(line, '#');
		if (p) *p = 0;
		char *save, *a = strtok_r(line, " \t\n", &save), *n;
		if (!a || !(n = strtok_r(0, " \t\n", &save))) continue;
		struct address ad;
		if (__lookup_ipliteral(&ad, a, AF_UNSPEC) != 1) continue;
		int k = 0;
		char *t;
		while (k < 15 && (t = strtok_r(0, " \t\n", &save))) aliases[k++] = t;
		aliases[k] = 0;
		memcpy(addr, ad.addr, 16);
		list[0] = (char *)addr;
		list[1] = 0;
		static_h.h_name = n;
		static_h.h_aliases = aliases;
		static_h.h_addrtype = ad.family;
		static_h.h_length = ad.family == AF_INET ? 4 : 16;
		static_h.h_addr_list = list;
		return &static_h;
	}
	if (!hosts_stay) endhostent();
	return 0;
}
