/* lib-spfxd — the exec family. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "syscall.h"

int execve(const char *path, char *const argv[], char *const envp[])
{
	return (int)__sysret(SYS_execve, path, argv, envp);
}

int execv(const char *path, char *const argv[])
{
	return execve(path, argv, __environ);
}

int fexecve(int fd, char *const argv[], char *const envp[])
{
	long r = __syscall(SYS_execveat, fd, "", argv, envp, AT_EMPTY_PATH);
	if (r != -ENOSYS) return (int)__syscall_ret((unsigned long)r);
	errno = ENOSYS;
	return -1;
}

/* Run a file the kernel rejected as ENOEXEC as a shell script. */
static void exec_script(const char *file, char *const argv[], char *const envp[])
{
	size_t argc = 0;
	while (argv[argc]) argc++;
	char **nargv = __builtin_alloca((argc + 2) * sizeof *nargv);
	nargv[0] = (char *)"sh";
	nargv[1] = (char *)file;
	for (size_t i = 1; i <= argc; i++) nargv[i + 1] = argv[i];
	if (argc == 0) nargv[2] = 0;
	execve(_PATH_BSHELL, nargv, envp);
}

int execvpe(const char *file, char *const argv[], char *const envp[])
{
	if (!*file) {
		errno = ENOENT;
		return -1;
	}
	if (strchr(file, '/')) {
		execve(file, argv, envp);
		if (errno == ENOEXEC) exec_script(file, argv, envp);
		return -1;
	}
	const char *path = getenv("PATH");
	if (!path) path = "/usr/local/bin:/bin:/usr/bin";
	size_t flen = strnlen(file, NAME_MAX + 1);
	if (flen > NAME_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}
	char buf[PATH_MAX];
	int seen_eacces = 0;
	for (const char *p = path;; ) {
		const char *z = strchrnul(p, ':');
		size_t dlen = (size_t)(z - p);
		if (dlen + flen + 3 <= sizeof buf) {
			/* an empty PATH element means the current directory */
			size_t k = 0;
			if (dlen) {
				memcpy(buf, p, dlen);
				k = dlen;
			} else {
				buf[k++] = '.';
			}
			buf[k++] = '/';
			memcpy(buf + k, file, flen + 1);
			execve(buf, argv, envp);
			switch (errno) {
			case EACCES:
				seen_eacces = 1;
				/* fallthrough */
			case ENOENT:
			case ENOTDIR:
				break;
			case ENOEXEC:
				exec_script(buf, argv, envp);
				return -1;
			default:
				return -1;
			}
		}
		if (!*z) break;
		p = z + 1;
	}
	if (seen_eacces) errno = EACCES;
	return -1;
}

int execvp(const char *file, char *const argv[])
{
	return execvpe(file, argv, __environ);
}

/* Collect "arg0, ..., NULL" (and for execle the trailing envp). */
#define COLLECT_ARGS(arg0, ap, argv)                                     \
	size_t argc_;                                                         \
	va_start(ap, arg0);                                                   \
	for (argc_ = 1; va_arg(ap, char *); argc_++);                         \
	va_end(ap);                                                           \
	char **argv = __builtin_alloca((argc_ + 1) * sizeof(char *));        \
	va_start(ap, arg0);                                                   \
	argv[0] = (char *)arg0;                                               \
	for (size_t i_ = 1; i_ <= argc_; i_++) argv[i_] = va_arg(ap, char *)

int execl(const char *path, const char *arg0, ...)
{
	va_list ap;
	COLLECT_ARGS(arg0, ap, argv);
	va_end(ap);
	return execv(path, argv);
}

int execlp(const char *file, const char *arg0, ...)
{
	va_list ap;
	COLLECT_ARGS(arg0, ap, argv);
	va_end(ap);
	return execvp(file, argv);
}

int execle(const char *path, const char *arg0, ...)
{
	va_list ap;
	COLLECT_ARGS(arg0, ap, argv);
	char **envp = va_arg(ap, char **);
	va_end(ap);
	return execve(path, argv, envp);
}
