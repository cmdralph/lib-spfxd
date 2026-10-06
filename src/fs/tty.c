/* lib-spfxd — isatty / ttyname / ttyname_r / ctermid. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pthread_impl.h"

int isatty(int fd)
{
	struct winsize ws;
	long r = __syscall(SYS_ioctl, fd, TIOCGWINSZ, &ws);
	if (r == 0) return 1;
	errno = r == -EBADF ? EBADF : ENOTTY;
	return 0;
}

int ttyname_r(int fd, char *buf, size_t n)
{
	char proc[32];
	struct stat a, b;
	if (!isatty(fd)) return errno;
	snprintf(proc, sizeof proc, "/proc/self/fd/%d", fd);
	ssize_t l = readlink(proc, buf, n);
	if (l < 0) return errno;
	if ((size_t)l == n) return ERANGE;
	buf[l] = 0;
	/* The link may be stale (e.g. different mount namespace). */
	if (stat(buf, &a) || fstat(fd, &b) || a.st_ino != b.st_ino || a.st_dev != b.st_dev)
		return ENOENT;
	return 0;
}

char *ttyname(int fd)
{
	static char buf[TTY_NAME_MAX + 32];
	int r = ttyname_r(fd, buf, sizeof buf);
	if (r) {
		errno = r;
		return 0;
	}
	return buf;
}

char *ctermid(char *s)
{
	return s ? strcpy(s, "/dev/tty") : (char *)"/dev/tty";
}
