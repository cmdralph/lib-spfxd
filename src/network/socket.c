/* lib-spfxd — BSD socket system call wrappers (x86-64 has direct syscalls). */
#include <errno.h>
#include <sys/socket.h>
#include <string.h>
#include "syscall.h"

int socket(int d, int t, int p) { return (int)__sysret(SYS_socket, d, t, p); }
int socketpair(int d, int t, int p, int fd[2]) { return (int)__sysret(SYS_socketpair, d, t, p, fd); }
int shutdown(int fd, int how) { return (int)__sysret(SYS_shutdown, fd, how); }
int bind(int fd, const struct sockaddr *a, socklen_t l) { return (int)__sysret(SYS_bind, fd, a, l); }
int connect(int fd, const struct sockaddr *a, socklen_t l) { return (int)__sysret_cp(SYS_connect, fd, a, l); }
int listen(int fd, int backlog) { return (int)__sysret(SYS_listen, fd, backlog); }
int accept(int fd, struct sockaddr *restrict a, socklen_t *restrict l) { return (int)__sysret_cp(SYS_accept, fd, a, l); }
int accept4(int fd, struct sockaddr *restrict a, socklen_t *restrict l, int fl) { return (int)__sysret_cp(SYS_accept4, fd, a, l, fl); }
int getsockname(int fd, struct sockaddr *restrict a, socklen_t *restrict l) { return (int)__sysret(SYS_getsockname, fd, a, l); }
int getpeername(int fd, struct sockaddr *restrict a, socklen_t *restrict l) { return (int)__sysret(SYS_getpeername, fd, a, l); }
ssize_t send(int fd, const void *b, size_t n, int fl) { return __sysret_cp(SYS_sendto, fd, b, n, fl, 0, 0); }
ssize_t recv(int fd, void *b, size_t n, int fl) { return __sysret_cp(SYS_recvfrom, fd, b, n, fl, 0, 0); }
ssize_t sendto(int fd, const void *b, size_t n, int fl, const struct sockaddr *a, socklen_t l)
{
	return __sysret_cp(SYS_sendto, fd, b, n, fl, a, l);
}
ssize_t recvfrom(int fd, void *restrict b, size_t n, int fl, struct sockaddr *restrict a, socklen_t *restrict l)
{
	return __sysret_cp(SYS_recvfrom, fd, b, n, fl, a, l);
}
ssize_t sendmsg(int fd, const struct msghdr *m, int fl) { return __sysret_cp(SYS_sendmsg, fd, m, fl); }
ssize_t recvmsg(int fd, struct msghdr *m, int fl) { return __sysret_cp(SYS_recvmsg, fd, m, fl); }
int getsockopt(int fd, int lv, int on, void *restrict v, socklen_t *restrict l) { return (int)__sysret(SYS_getsockopt, fd, lv, on, v, l); }
int setsockopt(int fd, int lv, int on, const void *v, socklen_t l) { return (int)__sysret(SYS_setsockopt, fd, lv, on, v, l); }
int sendmmsg(int fd, struct mmsghdr *v, unsigned n, unsigned fl) { return (int)__sysret_cp(SYS_sendmmsg, fd, v, n, fl); }
int recvmmsg(int fd, struct mmsghdr *v, unsigned n, unsigned fl, struct timespec *t)
{
	return (int)__sysret_cp(SYS_recvmmsg, fd, v, n, fl, t);
}

int sockatmark(int fd)
{
	int r;
	if (__syscall_ret((unsigned long)__syscall(SYS_ioctl, fd, 0x8905 /* SIOCATMARK */, &r)) < 0) return -1;
	return r;
}
