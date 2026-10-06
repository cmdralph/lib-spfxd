/* lib-spfxd — <sys/socket.h> (Linux socket ABI) */
#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H
#include <features.h>
#define __SPFXD_NEED_socklen_t
#define __SPFXD_NEED_sa_family_t
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_ssize_t
#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_uid_t
#define __SPFXD_NEED_gid_t
#define __SPFXD_NEED_struct_iovec
#define __SPFXD_NEED_struct_timespec
#include <bits/typedefs.h>

struct msghdr {
	void *msg_name;
	socklen_t msg_namelen;
	struct iovec *msg_iov;
	int msg_iovlen, __pad1;
	void *msg_control;
	socklen_t msg_controllen, __pad2;
	int msg_flags;
};
struct cmsghdr {
	socklen_t cmsg_len;
	int __pad1;
	int cmsg_level;
	int cmsg_type;
};
#if defined(__SPFXD_GNU)
struct ucred { pid_t pid; uid_t uid; gid_t gid; };
struct mmsghdr { struct msghdr msg_hdr; unsigned int msg_len; };
#endif
struct linger { int l_onoff, l_linger; };
struct sockaddr { sa_family_t sa_family; char sa_data[14]; };
struct sockaddr_storage {
	sa_family_t ss_family;
	char __ss_padding[128 - sizeof(long) - sizeof(sa_family_t)];
	unsigned long __ss_align;
};

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

#define SOCK_STREAM    1
#define SOCK_DGRAM     2
#define SOCK_RAW       3
#define SOCK_RDM       4
#define SOCK_SEQPACKET 5
#define SOCK_DCCP      6
#define SOCK_PACKET    10
#define SOCK_CLOEXEC   02000000
#define SOCK_NONBLOCK  04000

#define PF_UNSPEC 0
#define PF_LOCAL  1
#define PF_UNIX   PF_LOCAL
#define PF_FILE   PF_LOCAL
#define PF_INET   2
#define PF_INET6  10
#define PF_NETLINK 16
#define PF_PACKET 17
#define AF_UNSPEC PF_UNSPEC
#define AF_LOCAL  PF_LOCAL
#define AF_UNIX   AF_LOCAL
#define AF_FILE   AF_LOCAL
#define AF_INET   PF_INET
#define AF_INET6  PF_INET6
#define AF_NETLINK PF_NETLINK
#define AF_PACKET PF_PACKET

#define SOL_SOCKET 1
#define SO_DEBUG 1
#define SO_REUSEADDR 2
#define SO_TYPE 3
#define SO_ERROR 4
#define SO_DONTROUTE 5
#define SO_BROADCAST 6
#define SO_SNDBUF 7
#define SO_RCVBUF 8
#define SO_KEEPALIVE 9
#define SO_OOBINLINE 10
#define SO_NO_CHECK 11
#define SO_PRIORITY 12
#define SO_LINGER 13
#define SO_BSDCOMPAT 14
#define SO_REUSEPORT 15
#define SO_PASSCRED 16
#define SO_PEERCRED 17
#define SO_RCVLOWAT 18
#define SO_SNDLOWAT 19
#define SO_RCVTIMEO 20
#define SO_SNDTIMEO 21
#define SO_ACCEPTCONN 30
#define SO_PROTOCOL 38
#define SO_DOMAIN 39
#define SOMAXCONN 4096

#define MSG_OOB       0x0001
#define MSG_PEEK      0x0002
#define MSG_DONTROUTE 0x0004
#define MSG_CTRUNC    0x0008
#define MSG_PROXY     0x0010
#define MSG_TRUNC     0x0020
#define MSG_DONTWAIT  0x0040
#define MSG_EOR       0x0080
#define MSG_WAITALL   0x0100
#define MSG_FIN       0x0200
#define MSG_SYN       0x0400
#define MSG_CONFIRM   0x0800
#define MSG_RST       0x1000
#define MSG_ERRQUEUE  0x2000
#define MSG_NOSIGNAL  0x4000
#define MSG_MORE      0x8000
#define MSG_WAITFORONE 0x10000
#define MSG_CMSG_CLOEXEC 0x40000000

#define SCM_RIGHTS 0x01
#define SCM_CREDENTIALS 0x02

#define __CMSG_LEN(c) (((c)->cmsg_len + sizeof(long) - 1) & ~(long)(sizeof(long) - 1))
#define __CMSG_NEXT(c) ((unsigned char *)(c) + __CMSG_LEN(c))
#define __MHDR_END(m) ((unsigned char *)(m)->msg_control + (m)->msg_controllen)
#define CMSG_DATA(c) ((unsigned char *)(((struct cmsghdr *)(c)) + 1))
#define CMSG_NXTHDR(m, c) \
	((c)->cmsg_len < sizeof(struct cmsghdr) || \
	 __CMSG_LEN(c) + sizeof(struct cmsghdr) >= (unsigned long)(__MHDR_END(m) - (unsigned char *)(c)) \
	 ? (struct cmsghdr *)0 : (struct cmsghdr *)__CMSG_NEXT(c))
#define CMSG_FIRSTHDR(m) ((unsigned long)(m)->msg_controllen >= sizeof(struct cmsghdr) ? \
	(struct cmsghdr *)(m)->msg_control : (struct cmsghdr *)0)
#define CMSG_ALIGN(len) (((len) + sizeof(long) - 1) & (unsigned long)~(sizeof(long) - 1))
#define CMSG_SPACE(len) (CMSG_ALIGN(len) + CMSG_ALIGN(sizeof(struct cmsghdr)))
#define CMSG_LEN(len)   (CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))

__SPFXD_BEGIN_DECLS
int socket(int, int, int);
int socketpair(int, int, int, int[2]);
int shutdown(int, int);
int bind(int, const struct sockaddr *, socklen_t);
int connect(int, const struct sockaddr *, socklen_t);
int listen(int, int);
int accept(int, struct sockaddr *__restrict, socklen_t *__restrict);
int accept4(int, struct sockaddr *__restrict, socklen_t *__restrict, int);
int getsockname(int, struct sockaddr *__restrict, socklen_t *__restrict);
int getpeername(int, struct sockaddr *__restrict, socklen_t *__restrict);
ssize_t send(int, const void *, size_t, int);
ssize_t recv(int, void *, size_t, int);
ssize_t sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
ssize_t recvfrom(int, void *__restrict, size_t, int, struct sockaddr *__restrict, socklen_t *__restrict);
ssize_t sendmsg(int, const struct msghdr *, int);
ssize_t recvmsg(int, struct msghdr *, int);
int getsockopt(int, int, int, void *__restrict, socklen_t *__restrict);
int setsockopt(int, int, int, const void *, socklen_t);
int sockatmark(int);
#if defined(__SPFXD_GNU)
int sendmmsg(int, struct mmsghdr *, unsigned int, unsigned int);
int recvmmsg(int, struct mmsghdr *, unsigned int, unsigned int, struct timespec *);
#endif
__SPFXD_END_DECLS
#endif
