/* lib-spfxd — <sys/shm.h> */
#ifndef _SYS_SHM_H
#define _SYS_SHM_H
#include <sys/ipc.h>
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_time_t
#include <bits/typedefs.h>

__SPFXD_BEGIN_DECLS

typedef unsigned long shmatt_t;

struct shmid_ds {
	struct ipc_perm shm_perm;
	size_t shm_segsz;
	time_t shm_atime;
	time_t shm_dtime;
	time_t shm_ctime;
	pid_t shm_cpid;
	pid_t shm_lpid;
	shmatt_t shm_nattch;
	unsigned long __unused4, __unused5;
};

#define SHM_RDONLY 010000
#define SHM_RND    020000
#define SHM_REMAP  040000
#define SHM_EXEC   0100000
#define SHM_LOCK   11
#define SHM_UNLOCK 12
#define SHM_STAT   13
#define SHM_INFO   14
#define SHMLBA     4096

void *shmat(int, const void *, int);
int shmctl(int, int, struct shmid_ds *);
int shmdt(const void *);
int shmget(key_t, size_t, int);

__SPFXD_END_DECLS
#endif
