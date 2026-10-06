/* lib-spfxd — perror: "prefix: message\n" written to stderr in one piece. */
#include <errno.h>
#include <string.h>
#include "stdio_impl.h"

void perror(const char *msg)
{
	FILE *f = &__stderr_FILE;
	const char *err = strerror(errno);
	char buf[512];
	size_t n = 0;
	if (msg && *msg) {
		size_t l = strlen(msg);
		if (l > sizeof buf - 3) l = sizeof buf - 3;
		memcpy(buf, msg, l);
		n = l;
		buf[n++] = ':';
		buf[n++] = ' ';
	}
	size_t el = strlen(err);
	if (el > sizeof buf - 1 - n) el = sizeof buf - 1 - n;
	memcpy(buf + n, err, el);
	n += el;
	buf[n++] = '\n';
	FLOCK(f);
	if (!f->mode) f->mode = -1;
	__fwritex((unsigned char *)buf, n, f);
	FUNLOCK(f);
}
