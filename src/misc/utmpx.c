/*
 * lib-spfxd — the user accounting database (utmpx).
 *
 * Records are read from and written to UTMPX_FILE (or the file named by
 * utmpxname) under fcntl record locks, so concurrent updates by other
 * programs are safe.  Writing requires permission to the file (normally
 * root or the utmp group); otherwise pututxline fails with errno set.
 */
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <utmpx.h>

_Static_assert(sizeof(struct utmpx) == 384, "utmpx record size");

static int fd = -1;
static char path[256] = UTMPX_FILE;
static struct utmpx last;

static int lock(int f, short type)
{
	struct flock fl = { .l_type = type, .l_whence = SEEK_SET };
	return fcntl(f, F_SETLKW, &fl);
}

static int ensure_open(int write)
{
	if (fd >= 0 && !write) return 0;
	if (fd >= 0) {
		/* reopen read-write if currently read-only */
		int fl = fcntl(fd, F_GETFL);
		if ((fl & O_ACCMODE) == O_RDWR) return 0;
		off_t pos = lseek(fd, 0, SEEK_CUR);
		int nfd = open(path, O_RDWR | O_CLOEXEC);
		if (nfd < 0) return -1;
		close(fd);
		fd = nfd;
		lseek(fd, pos, SEEK_SET);
		return 0;
	}
	fd = open(path, (write ? O_RDWR : O_RDONLY) | O_CLOEXEC);
	if (fd < 0 && !write) fd = open(path, O_RDONLY | O_CLOEXEC);
	return fd < 0 ? -1 : 0;
}

void setutxent(void)
{
	if (fd >= 0) lseek(fd, 0, SEEK_SET);
}

void endutxent(void)
{
	if (fd >= 0) close(fd);
	fd = -1;
}

int utmpxname(const char *file)
{
	size_t l = strlen(file);
	if (l >= sizeof path) {
		errno = ENAMETOOLONG;
		return -1;
	}
	endutxent();
	memcpy(path, file, l + 1);
	return 0;
}

static int read_one(struct utmpx *u)
{
	if (lock(fd, F_RDLCK) < 0 && errno != ENOLCK) return -1;
	ssize_t n = read(fd, u, sizeof *u);
	int e = errno;
	lock(fd, F_UNLCK);
	errno = e;
	return n == (ssize_t)sizeof *u ? 0 : -1;
}

struct utmpx *getutxent(void)
{
	if (ensure_open(0) < 0) return 0;
	if (read_one(&last) < 0) return 0;
	return &last;
}

static int id_match(const struct utmpx *a, const struct utmpx *b)
{
	switch (b->ut_type) {
	case RUN_LVL: case BOOT_TIME: case NEW_TIME: case OLD_TIME:
		return a->ut_type == b->ut_type;
	case INIT_PROCESS: case LOGIN_PROCESS: case USER_PROCESS: case DEAD_PROCESS:
		return (a->ut_type == INIT_PROCESS || a->ut_type == LOGIN_PROCESS ||
		        a->ut_type == USER_PROCESS || a->ut_type == DEAD_PROCESS) &&
		       !strncmp(a->ut_id, b->ut_id, sizeof a->ut_id);
	}
	return 0;
}

struct utmpx *getutxid(const struct utmpx *id)
{
	struct utmpx *u;
	while ((u = getutxent()))
		if (id_match(u, id)) return u;
	return 0;
}

struct utmpx *getutxline(const struct utmpx *line)
{
	struct utmpx *u;
	while ((u = getutxent()))
		if ((u->ut_type == LOGIN_PROCESS || u->ut_type == USER_PROCESS) &&
		    !strncmp(u->ut_line, line->ut_line, sizeof u->ut_line))
			return u;
	return 0;
}

struct utmpx *pututxline(const struct utmpx *ut)
{
	struct utmpx copy = *ut, tmp;
	if (ensure_open(1) < 0) return 0;
	if (lock(fd, F_WRLCK) < 0 && errno != ENOLCK) return 0;
	/* overwrite the matching entry, else append */
	off_t pos = -1;
	lseek(fd, 0, SEEK_SET);
	for (off_t off = 0; read(fd, &tmp, sizeof tmp) == (ssize_t)sizeof tmp; off += (off_t)sizeof tmp) {
		if (id_match(&tmp, &copy)) {
			pos = off;
			break;
		}
	}
	if (pos < 0) pos = lseek(fd, 0, SEEK_END);
	ssize_t n = pwrite(fd, &copy, sizeof copy, pos);
	int e = errno;
	lseek(fd, pos + (off_t)sizeof copy, SEEK_SET);
	lock(fd, F_UNLCK);
	if (n != (ssize_t)sizeof copy) {
		errno = n < 0 ? e : EIO;
		return 0;
	}
	last = copy;
	return &last;
}

void updwtmpx(const char *file, const struct utmpx *ut)
{
	int f = open(file, O_WRONLY | O_APPEND | O_CLOEXEC);
	if (f < 0) return;
	if (lock(f, F_WRLCK) == 0 || errno == ENOLCK) {
		ssize_t r = write(f, ut, sizeof *ut);
		(void)r;
		lock(f, F_UNLCK);
	}
	close(f);
}
