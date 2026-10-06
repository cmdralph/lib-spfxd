/* lib-spfxd — <spawn.h> */
#ifndef _SPAWN_H
#define _SPAWN_H
#include <features.h>
#define __SPFXD_NEED_mode_t
#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_sigset_t
#include <bits/typedefs.h>
#include <sched.h>

#define POSIX_SPAWN_RESETIDS      0x01
#define POSIX_SPAWN_SETPGROUP     0x02
#define POSIX_SPAWN_SETSIGDEF     0x04
#define POSIX_SPAWN_SETSIGMASK    0x08
#define POSIX_SPAWN_SETSCHEDPARAM 0x10
#define POSIX_SPAWN_SETSCHEDULER  0x20
#define POSIX_SPAWN_USEVFORK      0x40
#define POSIX_SPAWN_SETSID        0x80

typedef struct {
	int __flags;
	pid_t __pgrp;
	sigset_t __def, __mask;
	int __prio, __pol;
	int __pad[14];
} posix_spawnattr_t;

typedef struct {
	int __used, __cap;
	void *__actions;
	int __pad[12];
} posix_spawn_file_actions_t;

__SPFXD_BEGIN_DECLS
int posix_spawn(pid_t *__restrict, const char *__restrict, const posix_spawn_file_actions_t *,
	const posix_spawnattr_t *__restrict, char *const[__restrict], char *const[__restrict]);
int posix_spawnp(pid_t *__restrict, const char *__restrict, const posix_spawn_file_actions_t *,
	const posix_spawnattr_t *__restrict, char *const[__restrict], char *const[__restrict]);
int posix_spawnattr_init(posix_spawnattr_t *);
int posix_spawnattr_destroy(posix_spawnattr_t *);
int posix_spawnattr_setflags(posix_spawnattr_t *, short);
int posix_spawnattr_getflags(const posix_spawnattr_t *__restrict, short *__restrict);
int posix_spawnattr_setpgroup(posix_spawnattr_t *, pid_t);
int posix_spawnattr_getpgroup(const posix_spawnattr_t *__restrict, pid_t *__restrict);
int posix_spawnattr_setsigmask(posix_spawnattr_t *__restrict, const sigset_t *__restrict);
int posix_spawnattr_getsigmask(const posix_spawnattr_t *__restrict, sigset_t *__restrict);
int posix_spawnattr_setsigdefault(posix_spawnattr_t *__restrict, const sigset_t *__restrict);
int posix_spawnattr_getsigdefault(const posix_spawnattr_t *__restrict, sigset_t *__restrict);
int posix_spawnattr_setschedparam(posix_spawnattr_t *__restrict, const struct sched_param *__restrict);
int posix_spawnattr_getschedparam(const posix_spawnattr_t *__restrict, struct sched_param *__restrict);
int posix_spawnattr_setschedpolicy(posix_spawnattr_t *, int);
int posix_spawnattr_getschedpolicy(const posix_spawnattr_t *__restrict, int *__restrict);
int posix_spawn_file_actions_init(posix_spawn_file_actions_t *);
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *);
int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *__restrict, int, const char *__restrict, int, mode_t);
int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *, int);
int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *, int, int);
int posix_spawn_file_actions_addchdir_np(posix_spawn_file_actions_t *__restrict, const char *__restrict);
int posix_spawn_file_actions_addfchdir_np(posix_spawn_file_actions_t *, int);
__SPFXD_END_DECLS
#endif
