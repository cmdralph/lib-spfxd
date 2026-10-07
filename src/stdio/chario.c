/* lib-spfxd — character I/O: fgetc/getc/getchar, fputc/putc/putchar and
 * their _unlocked variants, ungetc. */
#include "stdio_impl.h"

int fgetc(FILE *f)
{
	if (!__libc.threaded) return getc_fast(f);
	FLOCK(f);
	int c = getc_fast(f);
	FUNLOCK(f);
	return c;
}
weak_alias(fgetc, getc);
weak_alias(fgetc, _IO_getc);

int getchar(void) { return fgetc(&__stdin_FILE); }

int fgetc_unlocked(FILE *f) { return getc_fast(f); }
weak_alias(fgetc_unlocked, getc_unlocked);
int getchar_unlocked(void) { return getc_fast(&__stdin_FILE); }

int fputc(int c, FILE *f)
{
	if (!__libc.threaded) return putc_fast(c, f);
	FLOCK(f);
	c = putc_fast(c, f);
	FUNLOCK(f);
	return c;
}
weak_alias(fputc, putc);
weak_alias(fputc, _IO_putc);

int putchar(int c) { return fputc(c, &__stdout_FILE); }

int fputc_unlocked(int c, FILE *f) { return putc_fast(c, f); }
weak_alias(fputc_unlocked, putc_unlocked);
int putchar_unlocked(int c) { return putc_fast(c, &__stdout_FILE); }

int ungetc(int c, FILE *f)
{
	if (c == EOF) return c;
	FLOCK(f);
	if (!f->rpos || f->wpos != f->wbase) __toread(f);
	if (!f->rpos || f->rpos <= f->buf - UNGET) {
		FUNLOCK(f);
		return EOF;
	}
	*--f->rpos = (unsigned char)c;
	f->flags &= ~F_EOF;
	FUNLOCK(f);
	return (unsigned char)c;
}

int getw(FILE *f)
{
	int x;
	return fread(&x, sizeof x, 1, f) ? x : EOF;
}

int putw(int x, FILE *f)
{
	return (int)fwrite(&x, sizeof x, 1, f) - 1;
}
