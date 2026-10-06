/* lib-spfxd — termios (ioctl-based) and pseudo-terminal helpers. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "syscall.h"

/* Kernel struct termios (TCGETS) lacks the speed fields; the speed lives
 * in c_cflag & CBAUD, which cfget/cfset manipulate directly. */
int tcgetattr(int fd, struct termios *t)
{
	if (__syscall_ret((unsigned long)__syscall(SYS_ioctl, fd, TCGETS, t)) < 0) return -1;
	t->__c_ispeed = t->__c_ospeed = t->c_cflag & CBAUD;
	return 0;
}

int tcsetattr(int fd, int act, const struct termios *t)
{
	if ((unsigned)act > TCSAFLUSH) {
		errno = EINVAL;
		return -1;
	}
	return ioctl(fd, TCSETS + (unsigned long)act, t);
}

speed_t cfgetospeed(const struct termios *t) { return t->c_cflag & CBAUD; }
speed_t cfgetispeed(const struct termios *t) { return cfgetospeed(t); }

int cfsetospeed(struct termios *t, speed_t s)
{
	if (s & ~CBAUD) {
		errno = EINVAL;
		return -1;
	}
	t->c_cflag = (t->c_cflag & ~CBAUD) | s;
	return 0;
}

int cfsetispeed(struct termios *t, speed_t s)
{
	return s ? cfsetospeed(t, s) : 0;
}

int cfsetspeed(struct termios *t, speed_t s)
{
	return cfsetospeed(t, s);
}

void cfmakeraw(struct termios *t)
{
	t->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	t->c_cflag &= ~(CSIZE | PARENB);
	t->c_cflag |= CS8;
	t->c_cc[VMIN] = 1;
	t->c_cc[VTIME] = 0;
}

int tcsendbreak(int fd, int dur) { return ioctl(fd, TCSBRKP, (long)dur); }
int tcdrain(int fd) { return (int)__sysret_cp(SYS_ioctl, fd, TCSBRK, 1); }
int tcflush(int fd, int q) { return ioctl(fd, TCFLSH, (long)q); }
int tcflow(int fd, int a) { return ioctl(fd, TCXONC, (long)a); }

pid_t tcgetsid(int fd)
{
	int sid;
	if (ioctl(fd, TIOCGSID, &sid) < 0) return -1;
	return sid;
}

pid_t tcgetpgrp(int fd)
{
	int pg;
	if (ioctl(fd, TIOCGPGRP, &pg) < 0) return -1;
	return pg;
}

int tcsetpgrp(int fd, pid_t pg)
{
	int p = pg;
	return ioctl(fd, TIOCSPGRP, &p);
}

int tcgetwinsize(int fd, struct winsize *w) { return ioctl(fd, TIOCGWINSZ, w); }
int tcsetwinsize(int fd, const struct winsize *w) { return ioctl(fd, TIOCSWINSZ, w); }

/* ---- pseudo-terminals ---- */

int posix_openpt(int flags)
{
	int r = open("/dev/ptmx", flags);
	if (r < 0 && errno == ENOSPC) errno = EAGAIN;
	return r;
}

int grantpt(int fd)
{
	return 0;   /* devpts creates slaves with the right owner and mode */
}

int unlockpt(int fd)
{
	int unlock = 0;
	return ioctl(fd, TIOCSPTLCK, &unlock);
}

int ptsname_r(int fd, char *buf, size_t len)
{
	int n;
	if (ioctl(fd, TIOCGPTN, &n)) return errno;
	if ((size_t)snprintf(buf, len, "/dev/pts/%d", n) >= len) return ERANGE;
	return 0;
}

char *ptsname(int fd)
{
	static char buf[32];
	int e = ptsname_r(fd, buf, sizeof buf);
	if (e) {
		errno = e;
		return 0;
	}
	return buf;
}
