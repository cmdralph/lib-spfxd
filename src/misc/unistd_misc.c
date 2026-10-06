/* lib-spfxd — getpass, daemon, getlogin, vhangup, getentropy users. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "syscall.h"

char *getpass(const char *prompt)
{
	static char buf[128];
	struct termios old, t;
	int fd = open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC);
	if (fd < 0) return 0;
	tcgetattr(fd, &t);
	old = t;
	t.c_lflag &= ~(ECHO | ISIG);
	t.c_lflag |= ICANON;
	t.c_iflag &= ~(INLCR | IGNCR);
	t.c_iflag |= ICRNL;
	tcsetattr(fd, TCSAFLUSH, &t);
	tcdrain(fd);
	dprintf(fd, "%s", prompt);
	ssize_t l = read(fd, buf, sizeof buf - 1);
	if (l >= 0) {
		if ((l > 0 && buf[l - 1] == '\n') || (size_t)l == sizeof buf - 1) l--;
		buf[l] = 0;
	}
	tcsetattr(fd, TCSAFLUSH, &old);
	dprintf(fd, "\n");
	close(fd);
	return l < 0 ? 0 : buf;
}

int daemon(int nochdir, int noclose)
{
	if (!nochdir && chdir("/")) return -1;
	if (!noclose) {
		int fd = open(_PATH_DEVNULL, O_RDWR);
		if (fd < 0) return -1;
		int failed = dup2(fd, 0) < 0 || dup2(fd, 1) < 0 || dup2(fd, 2) < 0;
		if (fd > 2) close(fd);
		if (failed) return -1;
	}
	switch (fork()) {
	case 0: break;
	case -1: return -1;
	default: _exit(0);
	}
	if (setsid() < 0) return -1;
	switch (fork()) {
	case 0: break;
	case -1: return -1;
	default: _exit(0);
	}
	return 0;
}

char *getlogin(void)
{
	return getenv("LOGNAME");
}

int getlogin_r(char *name, size_t size)
{
	char *l = getlogin();
	if (!l) return ENXIO;
	if (strlen(l) >= size) return ERANGE;
	strcpy(name, l);
	return 0;
}

int vhangup(void)
{
	return (int)__sysret(SYS_vhangup);
}
