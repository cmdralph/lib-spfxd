/* lib-spfxd — <poll.h> */
#ifndef _POLL_H
#define _POLL_H
#include <features.h>
#define __SPFXD_NEED_sigset_t
#define __SPFXD_NEED_struct_timespec
#define __SPFXD_NEED_time_t
#include <bits/typedefs.h>

#define POLLIN     0x001
#define POLLPRI    0x002
#define POLLOUT    0x004
#define POLLERR    0x008
#define POLLHUP    0x010
#define POLLNVAL   0x020
#define POLLRDNORM 0x040
#define POLLRDBAND 0x080
#define POLLWRNORM 0x100
#define POLLWRBAND 0x200
#define POLLMSG    0x400
#define POLLRDHUP  0x2000

typedef unsigned long nfds_t;
struct pollfd { int fd; short events; short revents; };

__SPFXD_BEGIN_DECLS
int poll(struct pollfd *, nfds_t, int);
#if defined(__SPFXD_GNU) || defined(__SPFXD_BSD)
int ppoll(struct pollfd *, nfds_t, const struct timespec *, const sigset_t *);
#endif
__SPFXD_END_DECLS
#endif
