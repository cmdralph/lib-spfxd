/* lib-spfxd — fclose. */
#include <stdlib.h>
#include "stdio_impl.h"

int fclose(FILE *f)
{
	int r;
	FLOCK(f);
	r = __fflush_unlocked(f);
	if (f->close) r |= f->close(f);
	FUNLOCK(f);

	if (f->flags & F_PERM) return r;

	FILE **head = __ofl_lock();
	if (f->prev) f->prev->next = f->next;
	if (f->next) f->next->prev = f->prev;
	if (*head == f) *head = f->next;
	__ofl_unlock();

	free(f->getln_buf);
	if (f->flags & F_ABUF) free(f->buf - UNGET);
	free(f);
	return r;
}
