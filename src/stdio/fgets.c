/* lib-spfxd — fgets: copies whole buffered runs up to the newline with
 * memchr/memcpy rather than character by character. */
#include <string.h>
#include "stdio_impl.h"

char *fgets(char *restrict s, int n, FILE *restrict f)
{
	char *p = s;
	int c = 0;
	if (n <= 0) return 0;
	FLOCK(f);
	if (!f->mode) f->mode = -1;
	size_t room = (size_t)n - 1;
	while (room) {
		if (f->rpos != f->rend) {
			size_t avail = (size_t)(f->rend - f->rpos);
			unsigned char *nl = memchr(f->rpos, '\n', avail);
			size_t k = nl ? (size_t)(nl - f->rpos) + 1 : avail;
			if (k > room) k = room;
			memcpy(p, f->rpos, k);
			f->rpos += k;
			p += k;
			room -= k;
			if (nl && (size_t)(nl - (f->rpos - k)) < k) break;
			continue;
		}
		c = __uflow(f);
		if (c == EOF) break;
		*p++ = (char)c;
		room--;
		if (c == '\n') break;
	}
	int err = (f->flags & F_ERR) && c == EOF;
	FUNLOCK(f);
	if ((p == s && n > 1) || err) return 0;
	*p = 0;
	return s;
}
weak_alias(fgets, fgets_unlocked);
