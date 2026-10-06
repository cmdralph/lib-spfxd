/* lib-spfxd — <aio.h> */
#ifndef _AIO_H
#define _AIO_H
#include <features.h>
#include <bits/siginfo.h>
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_ssize_t
#define __SPFXD_NEED_off_t
#define __SPFXD_NEED_struct_timespec
#include <bits/typedefs.h>

__SPFXD_BEGIN_DECLS

struct aiocb {
	int aio_fildes;
	int aio_lio_opcode;
	int aio_reqprio;
	volatile void *aio_buf;
	size_t aio_nbytes;
	struct sigevent aio_sigevent;
	off_t aio_offset;
	/* private */
	volatile int __err;
	ssize_t __ret;
	unsigned long __seq;
	void *__unused[6];
};

#define AIO_CANCELED    0
#define AIO_NOTCANCELED 1
#define AIO_ALLDONE     2

#define LIO_READ   0
#define LIO_WRITE  1
#define LIO_NOP    2

#define LIO_WAIT   0
#define LIO_NOWAIT 1

int aio_read(struct aiocb *);
int aio_write(struct aiocb *);
int aio_fsync(int, struct aiocb *);
int aio_error(const struct aiocb *);
ssize_t aio_return(struct aiocb *);
int aio_cancel(int, struct aiocb *);
int aio_suspend(const struct aiocb *const[], int, const struct timespec *);
int lio_listio(int, struct aiocb *__restrict const[__restrict], int, struct sigevent *__restrict);

__SPFXD_END_DECLS
#endif
