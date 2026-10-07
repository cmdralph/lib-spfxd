/* lib-spfxd — system(): run "/bin/sh -c cmd" with SIGINT/SIGQUIT ignored and
 * SIGCHLD blocked in the caller while the child runs (POSIX). */
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "pthread_impl.h"

int system(const char *cmd)
{
	if (!cmd) return 1;   /* a shell is available */

	struct sigaction ign, oldint, oldquit;
	sigset_t chld, oldmask, defaults;
	posix_spawnattr_t attr;
	pid_t pid;
	int status = -1, r;

	memset(&ign, 0, sizeof ign);
	ign.sa_handler = SIG_IGN;
	sigaction(SIGINT, &ign, &oldint);
	sigaction(SIGQUIT, &ign, &oldquit);
	sigemptyset(&chld);
	sigaddset(&chld, SIGCHLD);
	sigprocmask(SIG_BLOCK, &chld, &oldmask);

	sigemptyset(&defaults);
	if (oldint.sa_handler != SIG_IGN) sigaddset(&defaults, SIGINT);
	if (oldquit.sa_handler != SIG_IGN) sigaddset(&defaults, SIGQUIT);
	posix_spawnattr_init(&attr);
	posix_spawnattr_setsigmask(&attr, &oldmask);
	posix_spawnattr_setsigdefault(&attr, &defaults);
	posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK);

	char *argv[] = { (char *)"sh", (char *)"-c", (char *)cmd, 0 };
	r = posix_spawn(&pid, _PATH_BSHELL, 0, &attr, argv, __environ);
	posix_spawnattr_destroy(&attr);
	if (!r) {
		while (waitpid(pid, &status, 0) < 0 && errno == EINTR);
	} else {
		errno = r;
	}

	sigaction(SIGINT, &oldint, 0);
	sigaction(SIGQUIT, &oldquit, 0);
	sigprocmask(SIG_SETMASK, &oldmask, 0);
	return status;
}
