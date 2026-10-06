/*
 * lib-spfxd — syslog: RFC 3164-style messages over the /dev/log datagram
 * socket, with LOG_CONS fallback to /dev/console and LOG_PERROR echo.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include "lock.h"

static volatile int log_lock;
static const char *log_ident;
static int log_opt, log_facility = LOG_USER, log_mask = 0xff;
static int log_fd = -1;

int setlogmask(int mask)
{
	int old = log_mask;
	if (mask) log_mask = mask;
	return old;
}

static void connect_log(void)
{
	static const struct sockaddr_un addr = { AF_UNIX, "/dev/log" };
	if (log_fd < 0) log_fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (log_fd >= 0 && connect(log_fd, (const void *)&addr, sizeof addr) < 0) {
		close(log_fd);
		log_fd = -1;
	}
}

void openlog(const char *ident, int opt, int facility)
{
	__lock_always(&log_lock);
	log_ident = ident;
	log_opt = opt;
	log_facility = facility ? facility : LOG_USER;
	if ((opt & LOG_NDELAY) && log_fd < 0) connect_log();
	__unlock_always(&log_lock);
}

void closelog(void)
{
	__lock_always(&log_lock);
	if (log_fd >= 0) close(log_fd);
	log_fd = -1;
	__unlock_always(&log_lock);
}

void vsyslog(int prio, const char *fmt, va_list ap)
{
	char buf[1024], msg[900], ts[16];
	int saved = errno;
	if (!(log_mask & LOG_MASK(prio & 7)) || (prio & ~0x3ff)) return;
	__lock_always(&log_lock);
	if (!(prio & LOG_FACMASK)) prio |= log_facility;
	time_t now = time(0);
	struct tm tm;
	localtime_r(&now, &tm);
	strftime(ts, sizeof ts, "%b %e %T", &tm);
	errno = saved;
	vsnprintf(msg, sizeof msg, fmt, ap);
	const char *id = log_ident ? log_ident : program_invocation_short_name;
	int hl = snprintf(buf, sizeof buf, "<%d>%s %s", prio, ts, id ? id : "");
	if (log_opt & LOG_PID) hl += snprintf(buf + hl, sizeof buf - (size_t)hl, "[%d]", getpid());
	int l = hl + snprintf(buf + hl, sizeof buf - (size_t)hl, ": %s", msg);
	if (l >= (int)sizeof buf) l = sizeof buf - 1;
	if (log_fd < 0) connect_log();
	if (log_fd < 0 || send(log_fd, buf, (size_t)l, 0) < 0) {
		if (log_fd >= 0) {
			close(log_fd);
			log_fd = -1;
			connect_log();
		}
		if ((log_fd < 0 || send(log_fd, buf, (size_t)l, 0) < 0) && (log_opt & LOG_CONS)) {
			int fd = open("/dev/console", O_WRONLY | O_NOCTTY | O_CLOEXEC);
			if (fd >= 0) {
				dprintf(fd, "%.*s\r\n", l - hl, buf + hl);
				close(fd);
			}
		}
	}
	if (log_opt & LOG_PERROR) dprintf(2, "%.*s\n", l - hl, buf + hl);
	__unlock_always(&log_lock);
}

void syslog(int prio, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsyslog(prio, fmt, ap);
	va_end(ap);
}
