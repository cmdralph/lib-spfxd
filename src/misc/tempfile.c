/* lib-spfxd — mkstemp family, mkdtemp, tmpfile, tmpnam, tempnam. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "syscall.h"

static const char alnum[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

/* Replace six X's with random letters/digits. */
static void fill(char *x)
{
	unsigned char r[6];
	arc4random_buf(r, sizeof r);
	for (int i = 0; i < 6; i++) x[i] = alnum[r[i] % 62];
}

static char *xxxxxx(char *t, int suffix)
{
	size_t l = strlen(t);
	if ((size_t)suffix > l || l - (size_t)suffix < 6) return 0;
	char *x = t + l - suffix - 6;
	if (memcmp(x, "XXXXXX", 6)) return 0;
	return x;
}

int mkostemps(char *t, int suffix, int flags)
{
	char *x = xxxxxx(t, suffix);
	if (!x || (flags & ~(O_APPEND | O_CLOEXEC | O_DIRECT | O_DSYNC | O_NOATIME | O_SYNC))) {
		errno = EINVAL;
		return -1;
	}
	for (int tries = 0; tries < 100; tries++) {
		fill(x);
		int fd = open(t, O_RDWR | O_CREAT | O_EXCL | flags, 0600);
		if (fd >= 0) return fd;
		if (errno != EEXIST) break;
	}
	memcpy(x, "XXXXXX", 6);
	return -1;
}

int mkstemp(char *t) { return mkostemps(t, 0, 0); }
int mkostemp(char *t, int flags) { return mkostemps(t, 0, flags); }
int mkstemps(char *t, int suffix) { return mkostemps(t, suffix, 0); }

char *mkdtemp(char *t)
{
	char *x = xxxxxx(t, 0);
	if (!x) {
		errno = EINVAL;
		return 0;
	}
	for (int tries = 0; tries < 100; tries++) {
		fill(x);
		if (!mkdir(t, 0700)) return t;
		if (errno != EEXIST) break;
	}
	memcpy(x, "XXXXXX", 6);
	return 0;
}

FILE *tmpfile(void)
{
	int fd = open(P_tmpdir, O_RDWR | O_TMPFILE | O_CLOEXEC, 0600);
	if (fd < 0) {
		char name[] = P_tmpdir "/tmpfile_XXXXXX";
		fd = mkostemp(name, O_CLOEXEC);
		if (fd < 0) return 0;
		unlink(name);
	}
	FILE *f = fdopen(fd, "w+");
	if (!f) close(fd);
	return f;
}

char *tmpnam(char *buf)
{
	static char internal[L_tmpnam];
	char name[] = P_tmpdir "/tmpnam_XXXXXX";
	struct stat st;
	for (int tries = 0; tries < 100; tries++) {
		fill(name + sizeof name - 7);
		if (lstat(name, &st) && errno == ENOENT)
			return strcpy(buf ? buf : internal, name);
	}
	return 0;
}

char *tempnam(const char *dir, const char *pfx)
{
	char *d = getenv("TMPDIR");
	struct stat st;
	if (!d || access(d, W_OK)) d = (char *)dir;
	if (!d || access(d, W_OK)) d = (char *)P_tmpdir;
	if (!pfx) pfx = "temp_";
	size_t dl = strlen(d), pl = strnlen(pfx, 5);
	char *s = malloc(dl + pl + 9);
	if (!s) return 0;
	memcpy(s, d, dl);
	s[dl] = '/';
	memcpy(s + dl + 1, pfx, pl);
	memcpy(s + dl + 1 + pl, "XXXXXX", 7);
	for (int tries = 0; tries < 100; tries++) {
		fill(s + dl + 1 + pl);
		if (lstat(s, &st) && errno == ENOENT) return s;
	}
	free(s);
	return 0;
}
