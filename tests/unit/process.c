/* lib-spfxd test — processes: fork/wait status, exec family, posix_spawn
 * with file actions and attributes, pipes, vfork, atexit/on_exit order,
 * abort, system, process groups and resource queries. */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include "t.h"

extern char **environ;

static int read_all(int fd, char *buf, size_t n)
{
	size_t got = 0;
	ssize_t k;
	while (got < n - 1 && (k = read(fd, buf + got, n - 1 - got)) > 0) got += (size_t)k;
	buf[got] = 0;
	return (int)got;
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "child-exit")) return atoi(argv[2]);
	if (argc > 1 && !strcmp(argv[1], "child-env")) { printf("%s", getenv("SPFXD_CHILD") ? getenv("SPFXD_CHILD") : "none"); return 0; }

	int st;
	pid_t p = fork();
	if (!p) _exit(42);
	CHECK(p > 0 && waitpid(p, &st, 0) == p && WIFEXITED(st) && WEXITSTATUS(st) == 42, "fork/_exit/waitpid");
	p = fork();
	if (!p) { raise(SIGTERM); _exit(0); }
	waitpid(p, &st, 0);
	CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGTERM && !WIFEXITED(st), "terminated by signal");
	p = fork();
	if (!p) { raise(SIGSTOP); _exit(3); }
	waitpid(p, &st, WUNTRACED);
	CHECK(WIFSTOPPED(st) && WSTOPSIG(st) == SIGSTOP, "stopped");
	kill(p, SIGCONT);
	waitpid(p, &st, 0);
	CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 3, "continued then exited");
	CHECK(waitpid(-1, &st, WNOHANG) == -1 && errno == ECHILD, "no children left");
	siginfo_t si;
	p = fork();
	if (!p) _exit(9);
	CHECK(!waitid(P_PID, (id_t)p, &si, WEXITED) && si.si_pid == p && si.si_status == 9 && si.si_code == CLD_EXITED, "waitid");

	/* exec family */
	p = fork();
	if (!p) { execl(argv[0], argv[0], "child-exit", "7", (char *)0); _exit(100); }
	waitpid(p, &st, 0);
	CHECK(WEXITSTATUS(st) == 7, "execl");
	p = fork();
	if (!p) { execlp("sh", "sh", "-c", "exit 5", (char *)0); _exit(100); }
	waitpid(p, &st, 0);
	CHECK(WEXITSTATUS(st) == 5, "execlp searches PATH");
	errno = 0;
	CHECK(execve("/nonexistent/prog", argv, environ) == -1 && errno == ENOENT, "execve ENOENT");
	errno = 0;
	CHECK(execvp("spfxd-no-such-program", argv) == -1 && errno == ENOENT, "execvp ENOENT");

	/* pipe + dup2 + exec */
	int fds[2];
	CHECK(!pipe2(fds, O_CLOEXEC), "pipe2");
	p = fork();
	if (!p) {
		dup2(fds[1], 1);
		char *const env[] = { "SPFXD_CHILD=hello", 0 };
		execle(argv[0], argv[0], "child-env", (char *)0, env);
		_exit(100);
	}
	close(fds[1]);
	char buf[256];
	read_all(fds[0], buf, sizeof buf);
	close(fds[0]);
	waitpid(p, &st, 0);
	CHECK(!strcmp(buf, "hello"), "execle environment: '%s'", buf);

	/* posix_spawn with file actions and attributes */
	pipe(fds);
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, fds[1], 1);
	posix_spawn_file_actions_addclose(&fa, fds[0]);
	posix_spawnattr_t attr;
	posix_spawnattr_init(&attr);
	sigset_t mask;
	sigemptyset(&mask);
	posix_spawnattr_setsigmask(&attr, &mask);
	posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETPGROUP);
	posix_spawnattr_setpgroup(&attr, 0);
	char *sargv[] = { "sh", "-c", "echo spawned $$; exit 4", 0 };
	CHECK(!posix_spawnp(&p, "sh", &fa, &attr, sargv, environ), "posix_spawnp");
	close(fds[1]);
	read_all(fds[0], buf, sizeof buf);
	close(fds[0]);
	waitpid(p, &st, 0);
	CHECK(!strncmp(buf, "spawned ", 8) && atoi(buf + 8) == p && WEXITSTATUS(st) == 4, "spawn output '%s' st %d", buf, WEXITSTATUS(st));
	posix_spawn_file_actions_destroy(&fa);
	posix_spawnattr_destroy(&attr);
	char *bad[] = { "x", 0 };
	CHECK(posix_spawn(&p, "/nonexistent", 0, 0, bad, environ) == ENOENT, "posix_spawn reports exec failure");
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_addopen(&fa, 1, "/tmp/spfxd_spawn_out", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	char *eargv[] = { "sh", "-c", "echo file-action", 0 };
	CHECK(!posix_spawn(&p, "/bin/sh", &fa, 0, eargv, environ), "spawn with addopen");
	waitpid(p, &st, 0);
	int fd = open("/tmp/spfxd_spawn_out", O_RDONLY);
	read_all(fd, buf, sizeof buf);
	close(fd);
	unlink("/tmp/spfxd_spawn_out");
	CHECK(!strcmp(buf, "file-action\n"), "addopen output '%s'", buf);
	posix_spawn_file_actions_destroy(&fa);

	/* vfork */
	p = vfork();
	if (!p) _exit(11);
	waitpid(p, &st, 0);
	CHECK(WEXITSTATUS(st) == 11, "vfork");

	/* system */
	CHECK(WEXITSTATUS(system("exit 6")) == 6, "system status");
	CHECK(system(0) != 0, "system(NULL): shell available");

	/* abort raises SIGABRT even if caught handlers return */
	p = fork();
	if (!p) { signal(SIGABRT, SIG_IGN); abort(); }
	waitpid(p, &st, 0);
	CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT, "abort overrides SIG_IGN");

	/* exit-time ordering, observed through a pipe from a child */
	pipe(fds);
	p = fork();
	if (!p) {
		close(fds[0]);
		dup2(fds[1], 1);
		static int out;
		out = 1;
		void a1(void) { write(out, "1", 1); }
		void a2(void) { write(out, "2", 1); }
		void a3(int s, void *arg) { (void)arg; char c = (char)('0' + s); write(out, &c, 1); }
		atexit(a1);
		on_exit(a3, 0);
		atexit(a2);
		printf("buffered");                  /* flushed by exit */
		exit(5);
	}
	close(fds[1]);
	read_all(fds[0], buf, sizeof buf);
	close(fds[0]);
	waitpid(p, &st, 0);
	CHECK(!strcmp(buf, "251buffered") || !strcmp(buf, "251") || strstr(buf, "251"), "atexit order '%s'", buf);
	CHECK(strstr(buf, "buffered") != NULL, "stdio flushed at exit");

	/* ids, groups, sessions */
	CHECK(getpid() > 0 && getppid() > 0 && getpgrp() > 0, "ids");
	p = fork();
	if (!p) _exit(setsid() > 0 && getsid(0) == getpid() ? 0 : 1);
	waitpid(p, &st, 0);
	CHECK(WEXITSTATUS(st) == 0, "setsid in child");
	struct rlimit rl;
	CHECK(!getrlimit(RLIMIT_NOFILE, &rl) && rl.rlim_cur > 0, "getrlimit");
	struct rusage ru;
	CHECK(!getrusage(RUSAGE_SELF, &ru), "getrusage");
	CHECK(getpriority(PRIO_PROCESS, 0) >= -20 || errno == 0, "getpriority");
	return DONE();
}
