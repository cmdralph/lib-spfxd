/*
 * lib-spfxd — directory streams.  Entries are returned directly out of
 * the getdents64 buffer (struct dirent matches struct linux_dirent64).
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include "lock.h"

#define DIRBUF 4096

struct __spfxd_dir {
	int fd;
	size_t pos, len;
	off_t tell;
	volatile int lock;
	unsigned char buf[DIRBUF] __attribute__((__aligned__(8)));
};

ssize_t getdents64(int fd, void *buf, size_t n)
{
	if (n > INT_MAX) n = INT_MAX;
	return __sysret(SYS_getdents64, fd, buf, n);
}

DIR *fdopendir(int fd)
{
	long fl = __syscall(SYS_fcntl, fd, F_GETFL);
	if (fl < 0) {
		errno = (int)-fl;
		return 0;
	}
	if (fl & O_PATH) {
		errno = EBADF;
		return 0;
	}
	DIR *d = calloc(1, sizeof *d);
	if (!d) return 0;
	__syscall(SYS_fcntl, fd, F_SETFD, FD_CLOEXEC);
	d->fd = fd;
	return d;
}

DIR *opendir(const char *name)
{
	int fd = open(name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) return 0;
	DIR *d = calloc(1, sizeof *d);
	if (!d) {
		close(fd);
		return 0;
	}
	d->fd = fd;
	return d;
}

int closedir(DIR *d)
{
	int r = close(d->fd);
	free(d);
	return r;
}

struct dirent *readdir(DIR *d)
{
	if (d->pos >= d->len) {
		long n = __syscall(SYS_getdents64, d->fd, d->buf, sizeof d->buf);
		if (n <= 0) {
			/* end of directory leaves errno untouched */
			if (n < 0 && n != -ENOENT) errno = (int)-n;
			return 0;
		}
		d->len = (size_t)n;
		d->pos = 0;
	}
	struct dirent *de = (struct dirent *)(d->buf + d->pos);
	d->pos += de->d_reclen;
	d->tell = de->d_off;
	return de;
}
weak_alias(readdir, readdir64);

int readdir_r(DIR *restrict d, struct dirent *restrict buf, struct dirent **restrict res)
{
	int saved = errno;
	__lock_always(&d->lock);
	errno = 0;
	struct dirent *de = readdir(d);
	int err = errno;
	if (de) memcpy(buf, de, de->d_reclen);
	__unlock_always(&d->lock);
	errno = saved;
	if (err) return err;
	*res = de ? buf : 0;
	return 0;
}

void rewinddir(DIR *d)
{
	__lock_always(&d->lock);
	lseek(d->fd, 0, SEEK_SET);
	d->pos = d->len = 0;
	d->tell = 0;
	__unlock_always(&d->lock);
}

void seekdir(DIR *d, long off)
{
	__lock_always(&d->lock);
	d->tell = lseek(d->fd, off, SEEK_SET);
	d->pos = d->len = 0;
	__unlock_always(&d->lock);
}

long telldir(DIR *d)
{
	return d->tell;
}

int dirfd(DIR *d)
{
	return d->fd;
}

int alphasort(const struct dirent **a, const struct dirent **b)
{
	return strcoll((*a)->d_name, (*b)->d_name);
}

int versionsort(const struct dirent **a, const struct dirent **b)
{
	return strverscmp((*a)->d_name, (*b)->d_name);
}

int scandir(const char *path, struct dirent ***res,
	int (*sel)(const struct dirent *),
	int (*cmp)(const struct dirent **, const struct dirent **))
{
	DIR *d = opendir(path);
	struct dirent *de, **names = 0, **tmp;
	size_t cnt = 0, cap = 0;
	int saved = errno, fail = 0;
	if (!d) return -1;

	errno = 0;
	while ((de = readdir(d))) {
		if (sel && !sel(de)) continue;
		if (cnt >= cap) {
			cap = cap ? 2 * cap : 32;
			tmp = cap > INT_MAX ? 0 : realloc(names, cap * sizeof *names);
			if (!tmp) {
				fail = ENOMEM;
				break;
			}
			names = tmp;
		}
		names[cnt] = malloc(de->d_reclen);
		if (!names[cnt]) {
			fail = ENOMEM;
			break;
		}
		memcpy(names[cnt++], de, de->d_reclen);
	}
	if (!fail) fail = errno;
	closedir(d);
	if (fail) {
		while (cnt--) free(names[cnt]);
		free(names);
		errno = fail;
		return -1;
	}
	errno = saved;
	if (cmp && cnt) qsort(names, cnt, sizeof *names, (int (*)(const void *, const void *))cmp);
	*res = names;
	return (int)cnt;
}
