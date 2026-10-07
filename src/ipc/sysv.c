/*
 * lib-spfxd — System V IPC: ftok, message queues, semaphores, shared
 * memory.  Thin wrappers over the x86-64 system calls (which always use
 * the 64-bit structure layouts declared in the headers).  msgsnd, msgrcv,
 * semop and semtimedop are cancellation points.
 */
#include <errno.h>
#include <stdarg.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "syscall.h"

key_t ftok(const char *path, int id)
{
	struct stat st;
	if (stat(path, &st) < 0) return -1;
	return (key_t)((st.st_ino & 0xffff) | ((st.st_dev & 0xff) << 16) | ((unsigned)(id & 0xff) << 24));
}

int msgget(key_t k, int flag)
{
	return (int)__sysret(SYS_msgget, k, flag);
}

int msgctl(int id, int cmd, struct msqid_ds *buf)
{
	return (int)__sysret(SYS_msgctl, id, cmd, buf);
}

int msgsnd(int id, const void *msg, size_t sz, int flag)
{
	return (int)__sysret_cp(SYS_msgsnd, id, msg, sz, flag);
}

ssize_t msgrcv(int id, void *msg, size_t sz, long type, int flag)
{
	return __sysret_cp(SYS_msgrcv, id, msg, sz, type, flag);
}

int semget(key_t k, int n, int flag)
{
	return (int)__sysret(SYS_semget, k, n, flag);
}

int semctl(int id, int num, int cmd, ...)
{
	unsigned long arg = 0;
	switch (cmd) {
	case SETVAL: case GETALL: case SETALL: case IPC_STAT: case IPC_SET:
	case IPC_INFO: case SEM_INFO: case SEM_STAT: {
		va_list ap;
		va_start(ap, cmd);
		arg = va_arg(ap, unsigned long);
		va_end(ap);
	}
	}
	return (int)__sysret(SYS_semctl, id, num, cmd, arg);
}

int semop(int id, struct sembuf *ops, size_t n)
{
	return (int)__sysret_cp(SYS_semtimedop, id, ops, n, 0);
}

int semtimedop(int id, struct sembuf *ops, size_t n, const struct timespec *ts)
{
	return (int)__sysret_cp(SYS_semtimedop, id, ops, n, ts);
}

int shmget(key_t k, size_t sz, int flag)
{
	return (int)__sysret(SYS_shmget, k, sz, flag);
}

void *shmat(int id, const void *addr, int flag)
{
	return (void *)__sysret(SYS_shmat, id, addr, flag);
}

int shmdt(const void *addr)
{
	return (int)__sysret(SYS_shmdt, addr);
}

int shmctl(int id, int cmd, struct shmid_ds *buf)
{
	return (int)__sysret(SYS_shmctl, id, cmd, buf);
}
