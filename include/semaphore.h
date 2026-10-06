/* lib-spfxd — <semaphore.h> */
#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H
#include <features.h>
#define __SPFXD_NEED_time_t
#define __SPFXD_NEED_clockid_t
#define __SPFXD_NEED_struct_timespec
#define __SPFXD_NEED_mode_t
#include <bits/typedefs.h>
#include <fcntl.h>

/* value: count of available units; waiters: blocked threads (lets sem_post
 * skip the futex wake syscall when nobody is waiting). */
typedef struct { volatile int __val; volatile int __waiters; int __shared; int __pad[5]; } sem_t;
#define SEM_FAILED ((sem_t *)0)

__SPFXD_BEGIN_DECLS
int sem_init(sem_t *, int, unsigned);
int sem_destroy(sem_t *);
int sem_post(sem_t *);
int sem_wait(sem_t *);
int sem_trywait(sem_t *);
int sem_timedwait(sem_t *__restrict, const struct timespec *__restrict);
int sem_clockwait(sem_t *__restrict, clockid_t, const struct timespec *__restrict);
int sem_getvalue(sem_t *__restrict, int *__restrict);
sem_t *sem_open(const char *, int, ...);
int sem_close(sem_t *);
int sem_unlink(const char *);
__SPFXD_END_DECLS
#endif
