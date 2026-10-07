/*
 * lib-spfxd — posix_spawn.
 *
 * The child is created with clone(CLONE_VM | CLONE_VFORK) on a private
 * stack: no page tables are copied (cheap even for huge parents) and the
 * parent sleeps until the child has exec'd or failed.  Before cloning, the
 * parent blocks all signals; the child resets every caught signal to its
 * default action before unblocking, so no handler of the parent can ever
 * run on the shared address space.  The child reports a failure through
 * shared memory, and it uses only raw system calls before exec.
 */
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "pthread_impl.h"


enum { FA_CLOSE, FA_DUP2, FA_OPEN, FA_CHDIR, FA_FCHDIR };

struct fa {
	int op, fd, srcfd, oflag;
	mode_t mode;
	char *path;
};

struct spawn_args {
	const char *path;
	char *const *argv, *const *envp;
	const posix_spawn_file_actions_t *fa;
	const posix_spawnattr_t *attr;
	sigset_t oldmask;
	int use_path;
	volatile int err;
};

#define CHECK(r) do { long r_ = (r); if (r_ < 0) { a->err = (int)-r_; goto fail; } } while (0)

static int child(void *p)
{
	struct spawn_args *a = p;
	const posix_spawnattr_t *at = a->attr;
	int flags = at ? at->__flags : 0;
	struct sigaction sa;

	/* Reset dispositions: caught signals -> default; explicitly requested
	 * defaults too.  Ignored signals stay ignored (POSIX). */
	memset(&sa, 0, sizeof sa);
	for (int sig = 1; sig < _NSIG; sig++) {
		struct sigaction old;
		if (sig == SIGKILL || sig == SIGSTOP) continue;
		int force = (flags & POSIX_SPAWN_SETSIGDEF) && sigismember(&at->__def, sig) > 0;
		if (sig == SIGCANCEL || sig == SIGSYNCCALL) force = 1;
		if (!force) {
			if (__libc_sigaction(sig, 0, &old)) continue;
			if (old.sa_handler == SIG_DFL || old.sa_handler == SIG_IGN) continue;
		}
		sa.sa_handler = SIG_DFL;
		__libc_sigaction(sig, &sa, 0);
	}

	if (flags & POSIX_SPAWN_SETSID) CHECK(__syscall(SYS_setsid));
	if (flags & POSIX_SPAWN_SETPGROUP) CHECK(__syscall(SYS_setpgid, 0, at->__pgrp));
	if (flags & POSIX_SPAWN_SETSCHEDULER) {
		struct sched_param sp = { .sched_priority = at->__prio };
		CHECK(__syscall(SYS_sched_setscheduler, 0, at->__pol, &sp));
	} else if (flags & POSIX_SPAWN_SETSCHEDPARAM) {
		struct sched_param sp = { .sched_priority = at->__prio };
		CHECK(__syscall(SYS_sched_setparam, 0, &sp));
	}
	if (flags & POSIX_SPAWN_RESETIDS) {
		CHECK(__syscall(SYS_setgid, __syscall(SYS_getgid)));
		CHECK(__syscall(SYS_setuid, __syscall(SYS_getuid)));
	}

	if (a->fa && a->fa->__actions) {
		const struct fa *f = a->fa->__actions;
		for (int i = 0; i < a->fa->__used; i++, f++) {
			long fd;
			switch (f->op) {
			case FA_CLOSE:
				__syscall(SYS_close, f->fd);
				break;
			case FA_DUP2:
				if (f->srcfd == f->fd) {
					/* POSIX: dup2 onto itself clears FD_CLOEXEC */
					long fl = __syscall(SYS_fcntl, f->fd, F_GETFD);
					CHECK(fl);
					CHECK(__syscall(SYS_fcntl, f->fd, F_SETFD, fl & ~FD_CLOEXEC));
				} else {
					CHECK(__syscall(SYS_dup3, f->srcfd, f->fd, 0));
				}
				break;
			case FA_OPEN:
				fd = __syscall(SYS_openat, AT_FDCWD, f->path, f->oflag, f->mode);
				CHECK(fd);
				if (fd != f->fd) {
					CHECK(__syscall(SYS_dup3, fd, f->fd, 0));
					__syscall(SYS_close, fd);
				}
				break;
			case FA_CHDIR:
				CHECK(__syscall(SYS_chdir, f->path));
				break;
			case FA_FCHDIR:
				CHECK(__syscall(SYS_fchdir, f->fd));
				break;
			}
		}
	}

	__syscall(SYS_rt_sigprocmask, SIG_SETMASK,
		(flags & POSIX_SPAWN_SETSIGMASK) ? &at->__mask : &a->oldmask, 0, 8);

	if (a->use_path) execvpe(a->path, a->argv, a->envp);
	else execve(a->path, a->argv, a->envp);
	a->err = errno;
fail:
	__syscall(SYS_exit_group, 127);
	return 0;
}

static int spawn(pid_t *restrict res, const char *restrict path,
	const posix_spawn_file_actions_t *fa, const posix_spawnattr_t *restrict attr,
	char *const argv[restrict], char *const envp[restrict], int use_path)
{
	struct spawn_args a = { path, argv, envp, fa, attr, { { 0 } }, use_path, 0 };
	size_t stack_size = 65536;
	int saved_errno = errno;
	unsigned char *stack = mmap(0, stack_size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
	if (stack == MAP_FAILED) return EAGAIN;

	int cs;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cs);
	__block_all_sigs(&a.oldmask);
	int pid = __clone(child, stack + stack_size, CLONE_VM | CLONE_VFORK | SIGCHLD, &a);
	__restore_sigs(&a.oldmask);
	pthread_setcancelstate(cs, 0);
	munmap(stack, stack_size);
	errno = saved_errno;

	if (pid < 0) return -pid;
	if (a.err) {
		int st;
		__syscall(SYS_wait4, pid, &st, 0, 0);
		return a.err;
	}
	if (res) *res = pid;
	return 0;
}

int posix_spawn(pid_t *restrict res, const char *restrict path,
	const posix_spawn_file_actions_t *fa, const posix_spawnattr_t *restrict attr,
	char *const argv[restrict], char *const envp[restrict])
{
	return spawn(res, path, fa, attr, argv, envp, 0);
}

int posix_spawnp(pid_t *restrict res, const char *restrict file,
	const posix_spawn_file_actions_t *fa, const posix_spawnattr_t *restrict attr,
	char *const argv[restrict], char *const envp[restrict])
{
	return spawn(res, file, fa, attr, argv, envp, 1);
}

/* ---- attributes ---- */

int posix_spawnattr_init(posix_spawnattr_t *a) { memset(a, 0, sizeof *a); return 0; }
int posix_spawnattr_destroy(posix_spawnattr_t *a) { return 0; }

int posix_spawnattr_setflags(posix_spawnattr_t *a, short f)
{
	if ((unsigned short)f & ~0xffU) return EINVAL;
	a->__flags = f;
	return 0;
}
int posix_spawnattr_getflags(const posix_spawnattr_t *restrict a, short *restrict f) { *f = (short)a->__flags; return 0; }
int posix_spawnattr_setpgroup(posix_spawnattr_t *a, pid_t p) { a->__pgrp = p; return 0; }
int posix_spawnattr_getpgroup(const posix_spawnattr_t *restrict a, pid_t *restrict p) { *p = a->__pgrp; return 0; }
int posix_spawnattr_setsigmask(posix_spawnattr_t *restrict a, const sigset_t *restrict m) { a->__mask = *m; return 0; }
int posix_spawnattr_getsigmask(const posix_spawnattr_t *restrict a, sigset_t *restrict m) { *m = a->__mask; return 0; }
int posix_spawnattr_setsigdefault(posix_spawnattr_t *restrict a, const sigset_t *restrict d) { a->__def = *d; return 0; }
int posix_spawnattr_getsigdefault(const posix_spawnattr_t *restrict a, sigset_t *restrict d) { *d = a->__def; return 0; }
int posix_spawnattr_setschedparam(posix_spawnattr_t *restrict a, const struct sched_param *restrict p) { a->__prio = p->sched_priority; return 0; }
int posix_spawnattr_getschedparam(const posix_spawnattr_t *restrict a, struct sched_param *restrict p)
{
	memset(p, 0, sizeof *p);
	p->sched_priority = a->__prio;
	return 0;
}
int posix_spawnattr_setschedpolicy(posix_spawnattr_t *a, int p) { a->__pol = p; return 0; }
int posix_spawnattr_getschedpolicy(const posix_spawnattr_t *restrict a, int *restrict p) { *p = a->__pol; return 0; }

/* ---- file actions ---- */

int posix_spawn_file_actions_init(posix_spawn_file_actions_t *fa)
{
	memset(fa, 0, sizeof *fa);
	return 0;
}

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fa)
{
	struct fa *f = fa->__actions;
	for (int i = 0; i < fa->__used; i++) free(f[i].path);
	free(f);
	fa->__actions = 0;
	fa->__used = fa->__cap = 0;
	return 0;
}

static struct fa *add(posix_spawn_file_actions_t *fa)
{
	if (fa->__used == fa->__cap) {
		int cap = fa->__cap ? fa->__cap * 2 : 8;
		struct fa *n = realloc(fa->__actions, (size_t)cap * sizeof *n);
		if (!n) return 0;
		fa->__actions = n;
		fa->__cap = cap;
	}
	struct fa *f = (struct fa *)fa->__actions + fa->__used++;
	memset(f, 0, sizeof *f);
	return f;
}

int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *fa, int fd)
{
	if (fd < 0) return EBADF;
	struct fa *f = add(fa);
	if (!f) return ENOMEM;
	f->op = FA_CLOSE;
	f->fd = fd;
	return 0;
}

int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *fa, int src, int fd)
{
	if (src < 0 || fd < 0) return EBADF;
	struct fa *f = add(fa);
	if (!f) return ENOMEM;
	f->op = FA_DUP2;
	f->srcfd = src;
	f->fd = fd;
	return 0;
}

static int add_path(posix_spawn_file_actions_t *fa, int op, int fd, const char *path, int oflag, mode_t mode)
{
	char *copy = strdup(path);
	if (!copy) return ENOMEM;
	struct fa *f = add(fa);
	if (!f) {
		free(copy);
		return ENOMEM;
	}
	f->op = op;
	f->fd = fd;
	f->path = copy;
	f->oflag = oflag;
	f->mode = mode;
	return 0;
}

int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *restrict fa, int fd,
	const char *restrict path, int oflag, mode_t mode)
{
	if (fd < 0) return EBADF;
	return add_path(fa, FA_OPEN, fd, path, oflag, mode);
}

int posix_spawn_file_actions_addchdir_np(posix_spawn_file_actions_t *restrict fa, const char *restrict path)
{
	return add_path(fa, FA_CHDIR, -1, path, 0, 0);
}

int posix_spawn_file_actions_addfchdir_np(posix_spawn_file_actions_t *fa, int fd)
{
	if (fd < 0) return EBADF;
	struct fa *f = add(fa);
	if (!f) return ENOMEM;
	f->op = FA_FCHDIR;
	f->fd = fd;
	return 0;
}
