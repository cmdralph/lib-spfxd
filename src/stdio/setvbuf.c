/*
 * lib-spfxd — setvbuf / setbuf / setbuffer / setlinebuf.
 *
 * A caller-supplied buffer gives up UNGET bytes at its start for ungetc
 * slack.  With a NULL buffer and a size larger than the current buffer, a
 * buffer of that size is allocated.  Must be used before any I/O.
 */
#include <stdlib.h>
#include "stdio_impl.h"

int setvbuf(FILE *restrict f, char *restrict buf, int mode, size_t size)
{
	int r = 0;
	if (mode != _IONBF && mode != _IOLBF && mode != _IOFBF) return -1;
	FLOCK(f);
	__fflush_unlocked(f);
	f->flags &= ~F_TTYCHK;
	f->lbf = mode == _IOLBF ? '\n' : EOF;
	if (mode == _IONBF) {
		f->buf_size = 0;
	} else if (buf && size > UNGET + 1) {
		if (f->flags & F_ABUF) free(f->buf - UNGET);
		f->flags = (f->flags & ~F_ABUF) | F_SVB;
		f->buf = (unsigned char *)buf + UNGET;
		f->buf_size = f->buf_cap = size - UNGET;
	} else if (size > f->buf_cap || !f->buf_cap) {
		size_t want = size > f->buf_cap ? size : BUFSIZ;
		unsigned char *nb = malloc(want + UNGET);
		if (nb) {
			if (f->flags & F_ABUF) free(f->buf - UNGET);
			f->flags |= F_ABUF;
			f->buf = nb + UNGET;
			f->buf_size = f->buf_cap = want;
		} else {
			r = -1;
		}
	} else {
		f->buf_size = f->buf_cap;
	}
	FUNLOCK(f);
	return r;
}

void setbuf(FILE *restrict f, char *restrict buf)
{
	setvbuf(f, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

void setbuffer(FILE *f, char *buf, size_t size)
{
	setvbuf(f, buf, buf ? _IOFBF : _IONBF, size);
}

void setlinebuf(FILE *f)
{
	setvbuf(f, 0, _IOLBF, 0);
}
