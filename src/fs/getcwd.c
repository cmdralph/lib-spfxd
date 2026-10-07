/* lib-spfxd — getcwd / get_current_dir_name. */
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "syscall.h"

static int kernel_getcwd(char *buf, size_t size)
{
	long r = __syscall(SYS_getcwd, buf, size);
	if (r < 0) {
		errno = (int)-r;
		return -1;
	}
	/* Linux reports an unreachable cwd as "(unreachable)/..." */
	if (r == 0 || buf[0] != '/') {
		errno = ENOENT;
		return -1;
	}
	return 0;
}

char *getcwd(char *buf, size_t size)
{
	if (!buf) {
		char tmp[PATH_MAX];
		if (kernel_getcwd(tmp, sizeof tmp)) return 0;
		return strdup(tmp);
	}
	if (!size) {
		errno = EINVAL;
		return 0;
	}
	return kernel_getcwd(buf, size) ? 0 : buf;
}

char *get_current_dir_name(void)
{
	struct stat a, b;
	char *pwd = getenv("PWD");
	if (pwd && *pwd == '/' && !stat(pwd, &a) && !stat(".", &b) &&
	    a.st_dev == b.st_dev && a.st_ino == b.st_ino)
		return strdup(pwd);
	return getcwd(0, 0);
}
