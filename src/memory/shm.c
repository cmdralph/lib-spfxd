/* lib-spfxd — POSIX shared memory objects (files in /dev/shm). */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int shm_path(const char *name, char *buf)
{
	while (*name == '/') name++;
	size_t n = strnlen(name, NAME_MAX + 1);
	if (!n || n > NAME_MAX || strchr(name, '/') || !strcmp(name, ".") || !strcmp(name, "..")) {
		errno = n > NAME_MAX ? ENAMETOOLONG : EINVAL;
		return -1;
	}
	memcpy(buf, "/dev/shm/", 9);
	memcpy(buf + 9, name, n + 1);
	return 0;
}

int shm_open(const char *name, int flag, mode_t mode)
{
	char path[NAME_MAX + 16];
	if (shm_path(name, path)) return -1;
	return open(path, flag | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK, mode);
}

int shm_unlink(const char *name)
{
	char path[NAME_MAX + 16];
	if (shm_path(name, path)) return -1;
	return unlink(path);
}
