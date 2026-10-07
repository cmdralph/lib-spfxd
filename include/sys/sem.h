/* lib-spfxd — <sys/sem.h> */
#ifndef _SYS_SEM_H
#define _SYS_SEM_H
#include <sys/ipc.h>
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_time_t
#define __SPFXD_NEED_struct_timespec
#include <bits/typedefs.h>

__SPFXD_BEGIN_DECLS

struct semid_ds {
	struct ipc_perm sem_perm;
	time_t sem_otime;
	unsigned long __unused1;
	time_t sem_ctime;
	unsigned long __unused2;
	unsigned long sem_nsems;
	unsigned long __unused3, __unused4;
};

struct sembuf {
	unsigned short sem_num;
	short sem_op;
	short sem_flg;
};

#define SEM_UNDO 0x1000
#define GETPID   11
#define GETVAL   12
#define GETALL   13
#define GETNCNT  14
#define GETZCNT  15
#define SETVAL   16
#define SETALL   17
#define SEM_STAT 18
#define SEM_INFO 19

int semctl(int, int, int, ...);
int semget(key_t, int, int);
int semop(int, struct sembuf *, size_t);
#ifdef __SPFXD_GNU
int semtimedop(int, struct sembuf *, size_t, const struct timespec *);
#endif

__SPFXD_END_DECLS
#endif
