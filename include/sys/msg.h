/* lib-spfxd — <sys/msg.h> */
#ifndef _SYS_MSG_H
#define _SYS_MSG_H
#include <sys/ipc.h>
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_ssize_t
#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_time_t
#include <bits/typedefs.h>

__SPFXD_BEGIN_DECLS

typedef unsigned long msgqnum_t;
typedef unsigned long msglen_t;

struct msqid_ds {
	struct ipc_perm msg_perm;
	time_t msg_stime;
	time_t msg_rtime;
	time_t msg_ctime;
	unsigned long __msg_cbytes;
	msgqnum_t msg_qnum;
	msglen_t msg_qbytes;
	pid_t msg_lspid;
	pid_t msg_lrpid;
	unsigned long __unused4, __unused5;
};

#define MSG_NOERROR 010000
#define MSG_EXCEPT  020000
#define MSG_COPY    040000
#define MSG_STAT    11
#define MSG_INFO    12

int msgctl(int, int, struct msqid_ds *);
int msgget(key_t, int);
ssize_t msgrcv(int, void *, size_t, long, int);
int msgsnd(int, const void *, size_t, int);

__SPFXD_END_DECLS
#endif
