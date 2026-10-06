/* lib-spfxd — list of open (non-standard) streams, for fflush(NULL) and exit. */
#include "stdio_impl.h"
#include "lock.h"

static FILE *ofl_head;
static volatile int ofl_lock;

hidden FILE **__ofl_lock(void)
{
	__lock(&ofl_lock);
	return &ofl_head;
}

hidden void __ofl_unlock(void)
{
	__unlock(&ofl_lock);
}

hidden FILE *__ofl_add(FILE *f)
{
	FILE **head = __ofl_lock();
	f->prev = 0;
	f->next = *head;
	if (*head) (*head)->prev = f;
	*head = f;
	__ofl_unlock();
	return f;
}

/* Called by exit(): write out pending output and, for input streams, move
 * the descriptor back to the stream's logical position (POSIX).  The list
 * lock and stream locks stay held: no other thread may use stdio after
 * this point. */
static void exit_file(FILE *f)
{
	FLOCK(f);
	(void)__need_unlock;
	if (f->wpos != f->wbase) f->write(f, 0, 0);
	if (f->rpos != f->rend && f->seek) f->seek(f, f->rpos - f->rend, SEEK_CUR);
	f->rpos = f->rend = f->wpos = f->wbase = f->wend = 0;
}

hidden void __stdio_exit(void)
{
	FILE **head = __ofl_lock();
	for (FILE *f = *head; f; f = f->next) exit_file(f);
	exit_file(&__stdin_FILE);
	exit_file(&__stdout_FILE);
	exit_file(&__stderr_FILE);
}
