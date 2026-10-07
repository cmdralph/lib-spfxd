/* lib-spfxd — fflush; fflush(NULL) flushes every output stream. */
#include "stdio_impl.h"

int fflush(FILE *f)
{
	int r = 0;
	if (f) {
		FLOCK(f);
		r = __fflush_unlocked(f);
		FUNLOCK(f);
		return r;
	}
	r |= fflush(&__stdout_FILE);
	r |= fflush(&__stderr_FILE);
	FILE **head = __ofl_lock();
	for (FILE *g = *head; g; g = g->next) {
		FLOCK(g);
		if (g->wpos != g->wbase) r |= __fflush_unlocked(g);
		FUNLOCK(g);
	}
	__ofl_unlock();
	return r;
}

int fflush_unlocked(FILE *f)
{
	return f ? __fflush_unlocked(f) : fflush(0);
}
