/* lib-spfxd — fseek / fseeko / ftell / ftello / rewind / fgetpos / fsetpos. */
#include <errno.h>
#include <limits.h>
#include "stdio_impl.h"

hidden off_t __ftello_unlocked(FILE *f)
{
	if (!f->seek) {
		errno = ESPIPE;
		return -1;
	}
	off_t pos = f->seek(f, 0, (f->flags & F_APP) && f->wpos != f->wbase ? SEEK_END : SEEK_CUR);
	if (pos < 0) return pos;
	if (f->rend) pos += f->rpos - f->rend;
	else if (f->wbase) pos += f->wpos - f->wbase;
	return pos;
}

off_t ftello(FILE *f)
{
	FLOCK(f);
	off_t r = __ftello_unlocked(f);
	FUNLOCK(f);
	return r;
}

long ftell(FILE *f)
{
	return (long)ftello(f);
}

hidden int __fseeko_unlocked(FILE *f, off_t off, int whence)
{
	if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
		errno = EINVAL;
		return -1;
	}
	if (!f->seek) {
		errno = ESPIPE;
		return -1;
	}
	if (whence == SEEK_CUR && f->rend) off -= f->rend - f->rpos;
	if (f->wpos != f->wbase) {
		f->write(f, 0, 0);
		if (!f->wpos) return -1;
	}
	f->wpos = f->wbase = f->wend = 0;
	if (f->seek(f, off, whence) < 0) return -1;
	f->rpos = f->rend = 0;
	f->flags &= ~F_EOF;
	return 0;
}

int fseeko(FILE *f, off_t off, int whence)
{
	FLOCK(f);
	int r = __fseeko_unlocked(f, off, whence);
	FUNLOCK(f);
	return r;
}

int fseek(FILE *f, long off, int whence)
{
	return fseeko(f, off, whence);
}

void rewind(FILE *f)
{
	FLOCK(f);
	__fseeko_unlocked(f, 0, SEEK_SET);
	f->flags &= ~F_ERR;
	FUNLOCK(f);
}

int fgetpos(FILE *restrict f, fpos_t *restrict pos)
{
	off_t off = ftello(f);
	if (off < 0) return -1;
	pos->__pos = off;
	pos->__mb[0] = f->mbs.__pending;
	pos->__mb[1] = f->mbs.__count;
	return 0;
}

int fsetpos(FILE *f, const fpos_t *pos)
{
	if (fseeko(f, (off_t)pos->__pos, SEEK_SET)) return -1;
	f->mbs.__pending = pos->__mb[0];
	f->mbs.__count = pos->__mb[1];
	return 0;
}
