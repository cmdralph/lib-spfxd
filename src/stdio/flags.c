/* lib-spfxd — feof / ferror / clearerr / fileno (+ _unlocked). */
#include <errno.h>
#include "stdio_impl.h"

int feof(FILE *f)
{
	FLOCK(f);
	int r = !!(f->flags & F_EOF);
	FUNLOCK(f);
	return r;
}

int ferror(FILE *f)
{
	FLOCK(f);
	int r = !!(f->flags & F_ERR);
	FUNLOCK(f);
	return r;
}

void clearerr(FILE *f)
{
	FLOCK(f);
	f->flags &= ~(F_EOF | F_ERR);
	FUNLOCK(f);
}

int fileno(FILE *f)
{
	FLOCK(f);
	int fd = f->fd;
	FUNLOCK(f);
	if (fd < 0) {
		errno = EBADF;
		return -1;
	}
	return fd;
}

int feof_unlocked(FILE *f) { return !!(f->flags & F_EOF); }
int ferror_unlocked(FILE *f) { return !!(f->flags & F_ERR); }
void clearerr_unlocked(FILE *f) { f->flags &= ~(F_EOF | F_ERR); }
int fileno_unlocked(FILE *f) { return f->fd < 0 ? (errno = EBADF, -1) : f->fd; }
