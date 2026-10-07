/* lib-spfxd — getdelim / getline / fgetln. */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "stdio_impl.h"

ssize_t getdelim(char **restrict s, size_t *restrict n, int delim, FILE *restrict f)
{
	size_t i = 0;
	if (!n || !s) {
		errno = EINVAL;
		return -1;
	}
	if (!*s) *n = 0;
	FLOCK(f);
	if (!f->mode) f->mode = -1;
	for (;;) {
		size_t avail = (size_t)(f->rend - f->rpos);
		unsigned char *z = f->rpos != f->rend ? memchr(f->rpos, delim, avail) : 0;
		size_t k = z ? (size_t)(z - f->rpos) + 1 : avail;
		if (i + k + 1 >= *n) {
			size_t m = i + k + 2;
			if (!z && m < SIZE_MAX / 4) m += m / 2;
			if (m < 128) m = 128;
			char *t = realloc(*s, m);
			if (!t) {
				f->flags |= F_ERR;
				FUNLOCK(f);
				errno = ENOMEM;
				return -1;
			}
			*s = t;
			*n = m;
		}
		if (k) {
			memcpy(*s + i, f->rpos, k);
			f->rpos += k;
			i += k;
		}
		if (z) break;
		int c = __uflow(f);
		if (c == EOF) {
			if (!i || (f->flags & F_ERR)) {
				FUNLOCK(f);
				return -1;
			}
			break;
		}
		/* __uflow consumed one byte: store it, unless it ends the line */
		if (i + 2 >= *n) {
			f->rpos--;
			continue;
		}
		(*s)[i++] = (char)c;
		if (c == delim) break;
	}
	(*s)[i] = 0;
	FUNLOCK(f);
	return (ssize_t)i;
}

ssize_t getline(char **restrict s, size_t *restrict n, FILE *restrict f)
{
	return getdelim(s, n, '\n', f);
}

char *fgetln(FILE *f, size_t *plen)
{
	size_t n = 0;
	ssize_t l = getline(&f->getln_buf, &n, f);
	if (l < 0) return 0;
	*plen = (size_t)l;
	return f->getln_buf;
}
