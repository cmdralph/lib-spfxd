/*
 * lib-spfxd — <stdio_ext.h>: inspection of a stream's buffer state, plus
 * fpurge.  These read the FILE directly and take no lock, as on other
 * implementations (callers lock if they need consistency).
 */
#include <stdio_ext.h>
#include <string.h>
#include "stdio_impl.h"

size_t __fbufsize(FILE *f) { return f->buf_size; }

size_t __fpending(FILE *f) { return f->wend ? (size_t)(f->wpos - f->wbase) : 0; }

int __flbf(FILE *f) { return f->lbf == '\n'; }

int __freadable(FILE *f) { return !(f->flags & F_NORD); }

int __fwritable(FILE *f) { return !(f->flags & F_NOWR); }

int __freading(FILE *f) { return (f->flags & F_NOWR) || f->rend; }

int __fwriting(FILE *f) { return (f->flags & F_NORD) || f->wend; }

/* Locking is always internal (per-call) in lib-spfxd; the request is
 * accepted and the previous state reported. */
int __fsetlocking(FILE *f, int type)
{
	(void)f;
	(void)type;
	return FSETLOCKING_INTERNAL;
}

void _flushlbf(void)
{
	fflush(0);
}

static void purge(FILE *f)
{
	f->rpos = f->rend = 0;
	f->wpos = f->wend = f->wbase = 0;
	memset(&f->mbs, 0, sizeof f->mbs);
}

void __fpurge(FILE *f)
{
	FLOCK(f);
	purge(f);
	FUNLOCK(f);
}

int fpurge(FILE *f)
{
	__fpurge(f);
	return 0;
}

void __fseterr(FILE *f)
{
	f->flags |= F_ERR;
}

size_t __freadahead(FILE *f)
{
	return f->rend ? (size_t)(f->rend - f->rpos) : 0;
}

const char *__freadptr(FILE *f, size_t *sz)
{
	if (!f->rend || f->rpos == f->rend) return 0;
	*sz = (size_t)(f->rend - f->rpos);
	return (const char *)f->rpos;
}

void __freadptrinc(FILE *f, size_t inc)
{
	f->rpos += inc;
}
