/* lib-spfxd — popen / pclose. */
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <spawn.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "stdio_impl.h"
#include "lock.h"

static volatile int popen_lock;
static FILE *popen_list[64];   /* streams the child must not inherit */

FILE *popen(const char *cmd, const char *mode)
{
	int p[2], op;
	posix_spawn_file_actions_t fa;
	FILE *f;
	pid_t pid;

	if (*mode == 'r') op = 0;
	else if (*mode == 'w') op = 1;
	else {
		errno = EINVAL;
		return 0;
	}
	if (pipe2(p, O_CLOEXEC)) return 0;
	f = fdopen(p[op], mode);
	if (!f) {
		close(p[0]);
		close(p[1]);
		return 0;
	}

	int e = posix_spawn_file_actions_init(&fa);
	__lock_always(&popen_lock);
	/* POSIX: streams from earlier popen calls are closed in the child */
	for (size_t i = 0; !e && i < sizeof popen_list / sizeof *popen_list; i++)
		if (popen_list[i]) e = posix_spawn_file_actions_addclose(&fa, popen_list[i]->fd);
	if (!e) e = posix_spawn_file_actions_adddup2(&fa, p[1 - op], 1 - op);
	if (!e) {
		char *argv[] = { (char *)"sh", (char *)"-c", (char *)cmd, 0 };
		e = posix_spawn(&pid, _PATH_BSHELL, &fa, 0, argv, __environ);
	}
	posix_spawn_file_actions_destroy(&fa);
	if (!e) {
		for (size_t i = 0; i < sizeof popen_list / sizeof *popen_list; i++)
			if (!popen_list[i]) {
				popen_list[i] = f;
				break;
			}
		f->pipe_pid = pid;
	}
	__unlock_always(&popen_lock);
	close(p[1 - op]);
	if (e) {
		fclose(f);
		errno = e;
		return 0;
	}
	if (!strchr(mode, 'e')) fcntl(p[op], F_SETFD, 0);
	return f;
}

int pclose(FILE *f)
{
	int status;
	pid_t pid = f->pipe_pid;
	__lock_always(&popen_lock);
	for (size_t i = 0; i < sizeof popen_list / sizeof *popen_list; i++)
		if (popen_list[i] == f) popen_list[i] = 0;
	__unlock_always(&popen_lock);
	fclose(f);
	while (waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR) return -1;
	}
	return status;
}
