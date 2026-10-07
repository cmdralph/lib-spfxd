/* lib-spfxd test — System V IPC, POSIX shared memory and message queues
 * (each skipped when the kernel or sandbox does not provide it). */
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include "t.h"

int main(void)
{
	int id = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600);
	if (id < 0) {
		SKIP("SysV shm unavailable");
	} else {
		char *p = shmat(id, 0, 0);
		CHECK(p != (void *)-1, "shmat");
		strcpy(p, "shared");
		struct shmid_ds ds;
		CHECK(!shmctl(id, IPC_STAT, &ds) && ds.shm_segsz == 4096 && ds.shm_nattch == 1, "shmctl IPC_STAT");
		CHECK(!shmdt(p) && !shmctl(id, IPC_RMID, 0), "shmdt/IPC_RMID");
	}
	int sid = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600);
	if (sid < 0) {
		SKIP("SysV semaphores unavailable");
	} else {
		CHECK(!semctl(sid, 0, SETVAL, 3) && semctl(sid, 0, GETVAL) == 3, "SETVAL/GETVAL");
		struct sembuf op = { 0, -2, 0 };
		CHECK(!semop(sid, &op, 1) && semctl(sid, 0, GETVAL) == 1, "semop");
		op.sem_flg = IPC_NOWAIT;
		CHECK(semop(sid, &op, 1) == -1 && errno == EAGAIN, "semop would block");
		CHECK(!semctl(sid, 0, IPC_RMID), "sem IPC_RMID");
	}
	int qid = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
	if (qid < 0) {
		SKIP("SysV message queues unavailable");
	} else {
		struct { long type; char text[16]; } m = { 5, "message" }, r;
		CHECK(!msgsnd(qid, &m, sizeof m.text, 0), "msgsnd");
		CHECK(msgrcv(qid, &r, sizeof r.text, 5, 0) == sizeof r.text && !strcmp(r.text, "message"), "msgrcv");
		CHECK(!msgctl(qid, IPC_RMID, 0), "msg IPC_RMID");
	}
	CHECK(ftok("/", 'x') != -1 && ftok("/nonexistent", 1) == -1, "ftok");
	int fd = shm_open("/spfxd_shm_test", O_RDWR | O_CREAT | O_EXCL, 0600);
	if (fd < 0) {
		SKIP("shm_open unavailable");
	} else {
		CHECK(!ftruncate(fd, 4096), "ftruncate shm");
		char *p = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		CHECK(p != MAP_FAILED, "mmap shm");
		CHECK(!shm_unlink("/spfxd_shm_test"), "shm_unlink");
		munmap(p, 4096);
		close(fd);
	}
	struct mq_attr at = { .mq_maxmsg = 4, .mq_msgsize = 32 };
	mqd_t q = mq_open("/spfxd_mq_test", O_RDWR | O_CREAT, 0600, &at);
	if (q == (mqd_t)-1) {
		SKIP("POSIX message queues unavailable");
	} else {
		CHECK(!mq_send(q, "low", 3, 1) && !mq_send(q, "high", 4, 9), "mq_send");
		char buf[32];
		unsigned prio;
		CHECK(mq_receive(q, buf, sizeof buf, &prio) == 4 && prio == 9 && !memcmp(buf, "high", 4), "priority order");
		struct mq_attr ga;
		CHECK(!mq_getattr(q, &ga) && ga.mq_curmsgs == 1 && ga.mq_maxmsg == 4, "mq_getattr");
		mq_close(q);
		CHECK(!mq_unlink("/spfxd_mq_test"), "mq_unlink");
	}
	return DONE();
}
