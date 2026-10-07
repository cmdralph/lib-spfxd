/*
 * lib-spfxd — ulimit and fmtmsg.
 */
#include <errno.h>
#include <fcntl.h>
#include <fmtmsg.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <ulimit.h>
#include <unistd.h>

/* File size limits in units of 512-byte blocks. */
long ulimit(int cmd, ...)
{
	struct rlimit rl;
	if (getrlimit(RLIMIT_FSIZE, &rl)) return -1;
	if (cmd == UL_GETFSIZE)
		return rl.rlim_cur == RLIM_INFINITY ? LONG_MAX : (long)(rl.rlim_cur / 512);
	if (cmd == UL_SETFSIZE) {
		va_list ap;
		va_start(ap, cmd);
		long v = va_arg(ap, long);
		va_end(ap);
		if (v < 0) {
			errno = EINVAL;
			return -1;
		}
		rl.rlim_cur = (rlim_t)v * 512;
		if (setrlimit(RLIMIT_FSIZE, &rl)) return -1;
		return v;
	}
	errno = EINVAL;
	return -1;
}

/*
 * fmtmsg: "label: severity: text\nTO FIX: action tag" on stderr
 * (MM_PRINT) and/or /dev/console (MM_CONSOLE).  MSGVERB (colon
 * separated list of label, severity, text, action, tag) selects the
 * components printed to stderr; an invalid MSGVERB prints all of them.
 */
static int verb_mask(void)
{
	static const char *const names[] = { "label", "severity", "text", "action", "tag" };
	const char *v = getenv("MSGVERB");
	if (!v || !*v) return 31;
	int m = 0;
	for (const char *p = v; *p;) {
		const char *e = strchr(p, ':');
		size_t l = e ? (size_t)(e - p) : strlen(p);
		int found = 0;
		for (int i = 0; i < 5; i++)
			if (strlen(names[i]) == l && !strncmp(p, names[i], l)) { m |= 1 << i; found = 1; }
		if (!found) return 31;
		p += l + (e ? 1 : 0);
	}
	return m;
}

static int emit(int fd, int mask, const char *label, int sev, const char *text,
                const char *action, const char *tag)
{
	static const char *const sevs[] = { "", "HALT", "ERROR", "WARNING", "INFO" };
	char buf[2048];
	size_t n = 0;
	const char *sevs_s = sev >= 0 && sev <= 4 ? sevs[sev] : 0;
	char sevbuf[24];
	if (!sevs_s) {
		snprintf(sevbuf, sizeof sevbuf, "SEV=%d", sev);
		sevs_s = sevbuf;
	}
#define ADD(...) (n += (size_t)snprintf(buf + n, n < sizeof buf ? sizeof buf - n : 0, __VA_ARGS__))
	int any = 0;
	if ((mask & 1) && label) { ADD("%s", label); any = 1; }
	if ((mask & 2) && sev) { ADD("%s%s", any ? ": " : "", sevs_s); any = 1; }
	if ((mask & 4) && text) { ADD("%s%s", any ? ": " : "", text); any = 1; }
	if ((mask & 8) && action) { ADD("%sTO FIX: %s", any ? "\n" : "", action); any = 1; }
	if ((mask & 16) && tag) { ADD("%s%s", any ? (mask & 8) && action ? "  " : "\n" : "", tag); any = 1; }
	if (!any) return 0;
	ADD("\n");
#undef ADD
	if (n >= sizeof buf) n = sizeof buf - 1;
	return write(fd, buf, n) == (ssize_t)n ? 0 : -1;
}

int fmtmsg(long cls, const char *label, int sev, const char *text, const char *action, const char *tag)
{
	int ret = MM_OK, failed_print = 0, failed_con = 0;
	if (label) {
		const char *c = strchr(label, ':');
		if (!c || c - label > 10 || strlen(c + 1) > 14) return MM_NOTOK;
	}
	if (cls & MM_PRINT) failed_print = emit(2, verb_mask(), label, sev, text, action, tag) < 0;
	if (cls & MM_CONSOLE) {
		int fd = open("/dev/console", O_WRONLY | O_NOCTTY | O_CLOEXEC);
		failed_con = fd < 0 || emit(fd, 31, label, sev, text, action, tag) < 0;
		if (fd >= 0) close(fd);
	}
	if (failed_print && failed_con) ret = MM_NOTOK;
	else if (failed_print) ret = MM_NOMSG;
	else if (failed_con) ret = MM_NOCON;
	return ret;
}
