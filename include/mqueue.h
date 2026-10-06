/* lib-spfxd — <mqueue.h> */
#ifndef _MQUEUE_H
#define _MQUEUE_H
#include <features.h>
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_ssize_t
#define __SPFXD_NEED_mode_t
#define __SPFXD_NEED_struct_timespec
#include <bits/typedefs.h>
#include <bits/siginfo.h>

__SPFXD_BEGIN_DECLS

typedef int mqd_t;

struct mq_attr {
	long mq_flags;
	long mq_maxmsg;
	long mq_msgsize;
	long mq_curmsgs;
	long __unused[4];
};

mqd_t mq_open(const char *, int, ...);
int mq_close(mqd_t);
int mq_unlink(const char *);
int mq_getattr(mqd_t, struct mq_attr *);
int mq_setattr(mqd_t, const struct mq_attr *__restrict, struct mq_attr *__restrict);
int mq_send(mqd_t, const char *, size_t, unsigned);
int mq_timedsend(mqd_t, const char *, size_t, unsigned, const struct timespec *);
ssize_t mq_receive(mqd_t, char *, size_t, unsigned *);
ssize_t mq_timedreceive(mqd_t, char *__restrict, size_t, unsigned *__restrict,
	const struct timespec *__restrict);
int mq_notify(mqd_t, const struct sigevent *);

__SPFXD_END_DECLS
#endif
