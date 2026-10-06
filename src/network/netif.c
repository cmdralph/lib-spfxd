/*
 * lib-spfxd — network interface names and indices (<net/if.h>).
 *
 * The kernel is asked directly with SIOCGIFINDEX / SIOCGIFNAME on a
 * throwaway datagram socket; if_nameindex enumerates /sys/class/net.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include "libc.h"

#define SIOCGIFNAME_  0x8910
#define SIOCGIFINDEX_ 0x8933

struct ifreq_ {
	char name[IF_NAMESIZE];
	union {
		int index;
		char pad[24];
	} u;
};

static int ctl_socket(void)
{
	int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd < 0) fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	return fd;
}

unsigned int if_nametoindex(const char *name)
{
	struct ifreq_ r;
	size_t l = strlen(name);
	if (l >= IF_NAMESIZE) {
		errno = ENODEV;
		return 0;
	}
	memset(&r, 0, sizeof r);
	memcpy(r.name, name, l);
	int fd = ctl_socket();
	if (fd < 0) return 0;
	int ok = ioctl(fd, SIOCGIFINDEX_, &r);
	close(fd);
	return ok < 0 ? 0 : (unsigned)r.u.index;
}

char *if_indextoname(unsigned int index, char *name)
{
	struct ifreq_ r;
	memset(&r, 0, sizeof r);
	r.u.index = (int)index;
	int fd = ctl_socket();
	if (fd < 0) return 0;
	int ok = ioctl(fd, SIOCGIFNAME_, &r);
	close(fd);
	if (ok < 0) {
		if (errno == ENODEV) errno = ENXIO;
		return 0;
	}
	memcpy(name, r.name, IF_NAMESIZE);
	name[IF_NAMESIZE - 1] = 0;
	return name;
}

static int by_index(const void *a, const void *b)
{
	const struct if_nameindex *x = a, *y = b;
	return (x->if_index > y->if_index) - (x->if_index < y->if_index);
}

struct if_nameindex *if_nameindex(void)
{
	DIR *d = opendir("/sys/class/net");
	if (!d) return 0;
	size_t n = 0, cap = 8;
	struct if_nameindex *v = malloc((cap + 1) * sizeof *v);
	struct dirent *e;
	if (!v) goto fail;
	while ((e = readdir(d))) {
		if (e->d_name[0] == '.') continue;
		unsigned idx = if_nametoindex(e->d_name);
		if (!idx) continue;
		if (n == cap) {
			struct if_nameindex *nv = realloc(v, (2 * cap + 1) * sizeof *v);
			if (!nv) goto fail;
			v = nv;
			cap *= 2;
		}
		v[n].if_index = idx;
		v[n].if_name = strdup(e->d_name);
		if (!v[n].if_name) goto fail;
		n++;
	}
	closedir(d);
	qsort(v, n, sizeof *v, by_index);
	v[n].if_index = 0;
	v[n].if_name = 0;
	return v;
fail:
	if (v) {
		v[n].if_name = 0;
		v[n].if_index = 0;
		if_freenameindex(v);
	}
	closedir(d);
	errno = ENOBUFS;
	return 0;
}

void if_freenameindex(struct if_nameindex *v)
{
	if (!v) return;
	for (struct if_nameindex *p = v; p->if_name; p++) free(p->if_name);
	free(v);
}
