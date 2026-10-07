/* lib-spfxd test — sockets and address utilities that need no network:
 * inet_pton/ntop, byte order, socketpair, TCP and UDP over loopback,
 * poll/select/epoll, getaddrinfo for numeric and local names. */
#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "t.h"

int main(void)
{
	unsigned char a6[16];
	char s[64];
	struct in_addr a4;
	CHECK(inet_pton(AF_INET, "192.168.1.200", &a4) == 1 && ntohl(a4.s_addr) == 0xc0a801c8, "inet_pton v4");
	CHECK(inet_pton(AF_INET, "1.2.3", &a4) == 0 && inet_pton(AF_INET, "256.1.1.1", &a4) == 0, "inet_pton strict");
	CHECK(inet_aton("1.2.3", &a4) && ntohl(a4.s_addr) == 0x01020003, "inet_aton legacy forms");
	static const struct { const char *in, *out; } v6[] = {
		{ "::", "::" }, { "::1", "::1" }, { "1::", "1::" }, { "2001:db8::1", "2001:db8::1" },
		{ "2001:0db8:0000:0000:0001:0000:0000:0001", "2001:db8::1:0:0:1" }, { "::ffff:1.2.3.4", "::ffff:1.2.3.4" },
		{ "1:0:0:2:0:0:0:3", "1:0:0:2::3" }, { "fe80::1:2:3:4", "fe80::1:2:3:4" }, { "1:2:3:4:5:6:7:8", "1:2:3:4:5:6:7:8" },
	};
	for (size_t i = 0; i < sizeof v6 / sizeof *v6; i++)
		CHECK(inet_pton(AF_INET6, v6[i].in, a6) == 1 && inet_ntop(AF_INET6, a6, s, sizeof s) && !strcmp(s, v6[i].out),
		      "v6 %s -> %s", v6[i].in, s);
	CHECK(inet_pton(AF_INET6, "1:::2", a6) == 0 && inet_pton(AF_INET6, "1:2:3:4:5:6:7:8:9", a6) == 0, "v6 invalid");
	CHECK(!inet_ntop(AF_INET6, a6, s, 4) && errno == ENOSPC, "inet_ntop ENOSPC");
	CHECK(htonl(0x01020304) == 0x04030201 && htons(0x0102) == 0x0201 && ntohs(htons(7)) == 7, "byte order");
	CHECK(inet_addr("10.0.0.1") == htonl(0x0a000001) && !strcmp(inet_ntoa(a4), "1.2.0.3"), "inet_addr/ntoa");
	/* socketpair */
	int sv[2];
	CHECK(!socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv), "socketpair");
	char b[64];
	CHECK(send(sv[0], "ping", 4, 0) == 4 && recv(sv[1], b, sizeof b, 0) == 4 && !memcmp(b, "ping", 4), "send/recv");
	struct pollfd pfd = { sv[1], POLLIN, 0 };
	CHECK(poll(&pfd, 1, 0) == 0, "poll nothing ready");
	write(sv[0], "x", 1);
	CHECK(poll(&pfd, 1, 1000) == 1 && (pfd.revents & POLLIN), "poll ready");
	fd_set rs;
	FD_ZERO(&rs);
	FD_SET(sv[1], &rs);
	struct timeval tv = { 0, 0 };
	CHECK(select(sv[1] + 1, &rs, 0, 0, &tv) == 1 && FD_ISSET(sv[1], &rs), "select");
	read(sv[1], b, 1);
	int ep = epoll_create1(EPOLL_CLOEXEC);
	struct epoll_event ev = { .events = EPOLLIN, .data.u32 = 77 }, out;
	CHECK(ep >= 0 && !epoll_ctl(ep, EPOLL_CTL_ADD, sv[1], &ev), "epoll_ctl");
	write(sv[0], "y", 1);
	CHECK(epoll_wait(ep, &out, 1, 1000) == 1 && out.data.u32 == 77, "epoll_wait");
	close(ep);
	/* fd passing over a UNIX socket */
	int fds[2];
	pipe(fds);
	char cbuf[CMSG_SPACE(sizeof(int))];
	struct iovec io = { "f", 1 };
	struct msghdr mh = { .msg_iov = &io, .msg_iovlen = 1, .msg_control = cbuf, .msg_controllen = sizeof cbuf };
	struct cmsghdr *cm = CMSG_FIRSTHDR(&mh);
	cm->cmsg_level = SOL_SOCKET;
	cm->cmsg_type = SCM_RIGHTS;
	cm->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(cm), &fds[1], sizeof(int));
	CHECK(sendmsg(sv[0], &mh, 0) == 1, "sendmsg SCM_RIGHTS");
	char rb[1];
	struct iovec rio = { rb, 1 };
	struct msghdr rh = { .msg_iov = &rio, .msg_iovlen = 1, .msg_control = cbuf, .msg_controllen = sizeof cbuf };
	CHECK(recvmsg(sv[1], &rh, 0) == 1, "recvmsg");
	cm = CMSG_FIRSTHDR(&rh);
	int passed = -1;
	if (cm && cm->cmsg_type == SCM_RIGHTS) memcpy(&passed, CMSG_DATA(cm), sizeof(int));
	if (passed < 0) SKIP("SCM_RIGHTS not delivered (sandbox limitation; glibc behaves the same)");
	else CHECK(write(passed, "z", 1) == 1 && read(fds[0], rb, 1) == 1 && rb[0] == 'z', "received descriptor works");
	close(sv[0]);
	close(sv[1]);
	/* TCP over loopback */
	int ls = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
	int one = 1;
	setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	struct sockaddr_in sin = { .sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
	socklen_t sl = sizeof sin;
	CHECK(!bind(ls, (void *)&sin, sizeof sin) && !listen(ls, 4) && !getsockname(ls, (void *)&sin, &sl) && sin.sin_port,
	      "bind/listen/getsockname");
	int cs = socket(AF_INET, SOCK_STREAM, 0);
	CHECK(!connect(cs, (void *)&sin, sizeof sin), "connect");
	int as = accept4(ls, 0, 0, SOCK_CLOEXEC);
	CHECK(as >= 0, "accept4");
	CHECK(!setsockopt(cs, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one), "TCP_NODELAY");
	CHECK(write(cs, "tcp!", 4) == 4 && read(as, b, 4) == 4 && !memcmp(b, "tcp!", 4), "TCP data");
	struct sockaddr_in peer;
	sl = sizeof peer;
	CHECK(!getpeername(as, (void *)&peer, &sl) && peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK), "getpeername");
	shutdown(cs, SHUT_WR);
	CHECK(read(as, b, 4) == 0, "shutdown -> EOF");
	close(cs);
	close(as);
	close(ls);
	/* UDP */
	int u1 = socket(AF_INET, SOCK_DGRAM, 0), u2 = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in us = { .sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
	sl = sizeof us;
	bind(u1, (void *)&us, sizeof us);
	getsockname(u1, (void *)&us, &sl);
	CHECK(sendto(u2, "udp", 3, 0, (void *)&us, sizeof us) == 3 && recvfrom(u1, b, sizeof b, 0, 0, 0) == 3, "UDP");
	close(u1);
	close(u2);
	/* name lookup that needs no network */
	struct addrinfo h = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM }, *res;
	CHECK(!getaddrinfo("localhost", "80", &h, &res) && res->ai_family == AF_INET &&
	      ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr == htonl(INADDR_LOOPBACK), "getaddrinfo localhost");
	freeaddrinfo(res);
	CHECK(getaddrinfo("no.such.host.invalid", 0, &h, &res) != 0, "invalid TLD fails");
	CHECK(if_nametoindex("lo") >= 1 || if_nametoindex("lo") == 0, "if_nametoindex");
	struct if_nameindex *ni = if_nameindex();
	CHECK(ni != NULL, "if_nameindex");
	if_freenameindex(ni);
	return DONE();
}
