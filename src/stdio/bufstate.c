/*
 * lib-spfxd — stream buffer state transitions: idle <-> reading <-> writing.
 */
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include "stdio_impl.h"
#include "syscall.h"

hidden int __toread(FILE *f)
{
	if (f->wpos != f->wbase) f->write(f, 0, 0);
	f->wpos = f->wbase = f->wend = 0;
	if (f->flags & F_NORD) {
		f->flags |= F_ERR;
		errno = EBADF;
		return EOF;
	}
	if (!f->rpos) f->rpos = f->rend = f->buf;
	return (f->flags & F_EOF) ? EOF : 0;
}

hidden int __towrite(FILE *f)
{
	if (f->flags & F_NOWR) {
		f->flags |= F_ERR;
		errno = EBADF;
		return EOF;
	}
	/* Pending input means the descriptor is ahead of the stream: rewind it
	 * so output lands at the stream's logical position. */
	if (f->rpos != f->rend && f->seek)
		f->seek(f, f->rpos - f->rend, SEEK_CUR);
	f->rpos = f->rend = 0;
	if (f->flags & F_TTYCHK) {
		struct winsize ws;
		f->flags &= ~F_TTYCHK;
		if (!__syscall(SYS_ioctl, f->fd, TIOCGWINSZ, &ws)) f->lbf = '\n';
	}
	f->wpos = f->wbase = f->buf;
	f->wend = f->buf + f->buf_size;
	return 0;
}

/* Reading stdin interactively must show pending prompts first. */
hidden void __stdin_refill_hook(FILE *f)
{
	FILE *out = &__stdout_FILE;
	if (f == &__stdin_FILE && out->lbf == '\n' && out->wpos != out->wbase) {
		FLOCK(out);
		__fflush_unlocked(out);
		FUNLOCK(out);
	}
}

hidden int __uflow(FILE *f)
{
	if (__toread(f)) return EOF;
	__stdin_refill_hook(f);
	size_t cap = f->buf_size ? f->buf_size : 1;
	size_t n = f->read(f, f->buf, cap);
	f->rpos = f->buf;
	f->rend = f->buf + n;
	if (!n) return EOF;
	return *f->rpos++;
}

hidden int __overflow(FILE *f, int ch)
{
	unsigned char c = (unsigned char)ch;
	if (!f->wend && __towrite(f)) return EOF;
	if (f->wpos != f->wend) {
		*f->wpos++ = c;
		if (c == f->lbf) {
			f->write(f, 0, 0);
			if (!f->wend) return EOF;
		}
		return c;
	}
	if (f->write(f, &c, 1) != 1) return EOF;
	return c;
}

hidden size_t __fwritex(const unsigned char *restrict s, size_t l, FILE *restrict f)
{
	size_t done = 0;
	if (!f->wend && __towrite(f)) return 0;
	if (l > (size_t)(f->wend - f->wpos)) return f->write(f, s, l);
	if (f->lbf >= 0) {
		const unsigned char *nl = memrchr(s, '\n', l);
		if (nl) {
			size_t k = (size_t)(nl - s) + 1;
			done = f->write(f, s, k);
			if (done < k) return done;
			s += k;
			l -= k;
		}
	}
	memcpy(f->wpos, s, l);
	f->wpos += l;
	return done + l;
}

hidden int __fflush_unlocked(FILE *f)
{
	if (f->wpos != f->wbase) {
		f->write(f, 0, 0);
		if (!f->wpos) return EOF;
	}
	if (f->rpos != f->rend) {
		/* Sync the descriptor with the stream position; on a pipe or
		 * terminal that is impossible, so keep the buffered input. */
		if (!f->seek || f->seek(f, f->rpos - f->rend, SEEK_CUR) < 0) {
			f->wpos = f->wbase = f->wend = 0;
			return 0;
		}
	}
	f->wpos = f->wbase = f->wend = 0;
	f->rpos = f->rend = 0;
	return 0;
}
