/* lib-spfxd — fmemopen, open_memstream, open_wmemstream, fopencookie. */
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "stdio_impl.h"

#define MSTREAM_BUF 1024

static FILE *new_stream(void *cookie, int rd, int wr, size_t bufsz)
{
	FILE *f = calloc(1, sizeof *f + UNGET + bufsz);
	if (!f) return 0;
	f->fd = -1;
	f->cookie = cookie;
	f->buf = (unsigned char *)(f + 1) + UNGET;
	f->buf_size = f->buf_cap = bufsz;
	f->lbf = EOF;
	if (!rd) f->flags |= F_NORD;
	if (!wr) f->flags |= F_NOWR;
	return __ofl_add(f);
}

/* Write the pending buffer [wbase, wpos), then data, through fn. */
static size_t flush_through(FILE *f, const unsigned char *data, size_t len,
	size_t (*fn)(void *, const unsigned char *, size_t))
{
	size_t pend = (size_t)(f->wpos - f->wbase);
	if (pend && fn(f->cookie, f->wbase, pend) != pend) goto fail;
	size_t done = len ? fn(f->cookie, data, len) : 0;
	f->wpos = f->wbase = f->buf;
	f->wend = f->buf + f->buf_size;
	if (done != len) goto fail;
	return len;
fail:
	f->wpos = f->wbase = f->wend = 0;
	f->flags |= F_ERR;
	return 0;
}

/* ---- fmemopen ---- */

struct fmem {
	unsigned char *buf;
	size_t size, pos, len;   /* len: current content length */
	int mode;                /* 'r', 'w', 'a' */
	int owned;
};

static size_t fmem_put(void *c, const unsigned char *s, size_t n)
{
	struct fmem *m = c;
	if (m->mode == 'a') m->pos = m->len;
	size_t room = m->size - m->pos;
	size_t k = n < room ? n : room;
	memcpy(m->buf + m->pos, s, k);
	m->pos += k;
	if (m->pos > m->len) m->len = m->pos;
	/* keep the content NUL terminated when there is room */
	if (m->len < m->size) m->buf[m->len] = 0;
	else if (k < n && m->size) m->buf[m->size - 1] = 0;
	return k;
}

static size_t fmem_write(FILE *f, const unsigned char *s, size_t n)
{
	return flush_through(f, s, n, fmem_put);
}

static size_t fmem_read(FILE *f, unsigned char *dst, size_t n)
{
	struct fmem *m = f->cookie;
	size_t avail = m->pos < m->len ? m->len - m->pos : 0;
	size_t k = n < avail ? n : avail;
	memcpy(dst, m->buf + m->pos, k);
	m->pos += k;
	if (!k) f->flags |= F_EOF;
	return k;
}

static off_t fmem_seek(FILE *f, off_t off, int whence)
{
	struct fmem *m = f->cookie;
	off_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (off_t)m->pos : (off_t)m->len;
	if (off < -base || off > (off_t)m->size - base) {
		errno = EINVAL;
		return -1;
	}
	m->pos = (size_t)(base + off);
	return (off_t)m->pos;
}

static int fmem_close(FILE *f)
{
	struct fmem *m = f->cookie;
	if (m->owned) free(m->buf);
	free(m);
	return 0;
}

FILE *fmemopen(void *restrict buf, size_t size, const char *restrict mode)
{
	if (!*mode || !strchr("rwa", *mode) || (!buf && !strchr(mode, '+')) || !size) {
		errno = EINVAL;
		return 0;
	}
	struct fmem *m = calloc(1, sizeof *m);
	if (!m) return 0;
	if (!buf) {
		buf = calloc(1, size);
		if (!buf) {
			free(m);
			return 0;
		}
		m->owned = 1;
	}
	m->buf = buf;
	m->size = size;
	m->mode = *mode;
	int plus = !!strchr(mode, '+');
	if (*mode == 'r') m->len = size;
	else if (*mode == 'w') m->buf[0] = 0;
	else m->pos = m->len = strnlen(buf, size);

	FILE *f = new_stream(m, *mode == 'r' || plus, *mode != 'r' || plus, MSTREAM_BUF);
	if (!f) {
		if (m->owned) free(m->buf);
		free(m);
		return 0;
	}
	f->read = fmem_read;
	f->write = fmem_write;
	f->seek = fmem_seek;
	f->close = fmem_close;
	return f;
}

/* ---- open_memstream ---- */

struct mstream {
	char **bufp;
	size_t *sizep;
	char *buf;
	size_t cap, pos, len;
};

static int ms_grow(struct mstream *m, size_t need)
{
	if (need < m->cap) return 0;
	size_t cap = m->cap ? m->cap : 64;
	while (cap <= need) {
		if (cap > SIZE_MAX / 2) return -1;
		cap *= 2;
	}
	char *n = realloc(m->buf, cap);
	if (!n) return -1;
	memset(n + m->cap, 0, cap - m->cap);
	m->buf = n;
	m->cap = cap;
	*m->bufp = n;
	return 0;
}

static size_t ms_put(void *c, const unsigned char *s, size_t n)
{
	struct mstream *m = c;
	if (n > SIZE_MAX - m->pos - 1 || ms_grow(m, m->pos + n + 1)) return 0;
	memcpy(m->buf + m->pos, s, n);
	m->pos += n;
	if (m->pos > m->len) m->len = m->pos;
	m->buf[m->len] = 0;
	*m->sizep = m->pos;
	return n;
}

static size_t ms_write(FILE *f, const unsigned char *s, size_t n)
{
	return flush_through(f, s, n, ms_put);
}

static off_t ms_seek(FILE *f, off_t off, int whence)
{
	struct mstream *m = f->cookie;
	off_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (off_t)m->pos : (off_t)m->len;
	if (off < -base || off > SSIZE_MAX - base) {
		errno = EINVAL;
		return -1;
	}
	size_t np = (size_t)(base + off);
	if (ms_grow(m, np + 1)) return -1;
	m->pos = np;
	*m->sizep = np < m->len ? np : m->len;
	return (off_t)np;
}

static int ms_close(FILE *f)
{
	free(f->cookie);
	return 0;
}

FILE *open_memstream(char **bufp, size_t *sizep)
{
	struct mstream *m = calloc(1, sizeof *m);
	if (!m) return 0;
	m->bufp = bufp;
	m->sizep = sizep;
	if (ms_grow(m, 1)) {
		free(m);
		return 0;
	}
	*sizep = 0;
	FILE *f = new_stream(m, 0, 1, MSTREAM_BUF);
	if (!f) {
		free(m->buf);
		free(m);
		return 0;
	}
	f->write = ms_write;
	f->seek = ms_seek;
	f->close = ms_close;
	return f;
}

/* ---- open_wmemstream: bytes written are decoded into wide characters ---- */

struct wmstream {
	wchar_t **bufp;
	size_t *sizep;
	wchar_t *buf;
	size_t cap, pos, len;
	mbstate_t st;
};

static int wms_grow(struct wmstream *m, size_t need)
{
	if (need < m->cap) return 0;
	size_t cap = m->cap ? m->cap : 64;
	while (cap <= need) {
		if (cap > SIZE_MAX / 2 / sizeof(wchar_t)) return -1;
		cap *= 2;
	}
	wchar_t *n = realloc(m->buf, cap * sizeof *n);
	if (!n) return -1;
	wmemset(n + m->cap, 0, cap - m->cap);
	m->buf = n;
	m->cap = cap;
	*m->bufp = n;
	return 0;
}

static size_t wms_put(void *c, const unsigned char *s, size_t n)
{
	struct wmstream *m = c;
	for (size_t i = 0; i < n; i++) {
		wchar_t wc;
		size_t r = mbrtowc(&wc, (const char *)s + i, 1, &m->st);
		if (r == (size_t)-2) continue;
		if (r == (size_t)-1) return i;
		if (wms_grow(m, m->pos + 2)) return i;
		m->buf[m->pos++] = wc;
		if (m->pos > m->len) m->len = m->pos;
		m->buf[m->len] = 0;
	}
	*m->sizep = m->pos;
	return n;
}

static size_t wms_write(FILE *f, const unsigned char *s, size_t n)
{
	return flush_through(f, s, n, wms_put);
}

static off_t wms_seek(FILE *f, off_t off, int whence)
{
	struct wmstream *m = f->cookie;
	off_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (off_t)m->pos : (off_t)m->len;
	if (off < -base || off > SSIZE_MAX / 4 - base) {
		errno = EINVAL;
		return -1;
	}
	size_t np = (size_t)(base + off);
	if (wms_grow(m, np + 1)) return -1;
	m->pos = np;
	memset(&m->st, 0, sizeof m->st);
	*m->sizep = np < m->len ? np : m->len;
	return (off_t)np;
}

FILE *open_wmemstream(wchar_t **bufp, size_t *sizep)
{
	struct wmstream *m = calloc(1, sizeof *m);
	if (!m) return 0;
	m->bufp = bufp;
	m->sizep = sizep;
	if (wms_grow(m, 1)) {
		free(m);
		return 0;
	}
	*sizep = 0;
	FILE *f = new_stream(m, 0, 1, MSTREAM_BUF);
	if (!f) {
		free(m->buf);
		free(m);
		return 0;
	}
	f->mode = 1;
	f->write = wms_write;
	f->seek = wms_seek;
	f->close = ms_close;
	return f;
}

/* ---- fopencookie ---- */

struct cookie {
	void *c;
	cookie_io_functions_t io;
};

static size_t ck_put(void *c, const unsigned char *s, size_t n)
{
	struct cookie *k = c;
	size_t done = 0;
	while (done < n) {
		ssize_t r = k->io.write ? k->io.write(k->c, (const char *)s + done, n - done) : -1;
		if (r <= 0) break;
		done += (size_t)r;
	}
	return done;
}

static size_t ck_write(FILE *f, const unsigned char *s, size_t n)
{
	return flush_through(f, s, n, ck_put);
}

static size_t ck_read(FILE *f, unsigned char *d, size_t n)
{
	struct cookie *k = f->cookie;
	ssize_t r = k->io.read ? k->io.read(k->c, (char *)d, n) : 0;
	if (r <= 0) {
		f->flags |= r ? F_ERR : F_EOF;
		return 0;
	}
	return (size_t)r;
}

static off_t ck_seek(FILE *f, off_t off, int whence)
{
	struct cookie *k = f->cookie;
	if (!k->io.seek) {
		errno = ENOTSUP;
		return -1;
	}
	if (k->io.seek(k->c, &off, whence) < 0) return -1;
	return off;
}

static int ck_close(FILE *f)
{
	struct cookie *k = f->cookie;
	int r = k->io.close ? k->io.close(k->c) : 0;
	free(k);
	return r;
}

FILE *fopencookie(void *c, const char *mode, cookie_io_functions_t io)
{
	if (!*mode || !strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}
	struct cookie *k = malloc(sizeof *k);
	if (!k) return 0;
	k->c = c;
	k->io = io;
	int plus = !!strchr(mode, '+');
	FILE *f = new_stream(k, *mode == 'r' || plus, *mode != 'r' || plus, BUFSIZ);
	if (!f) {
		free(k);
		return 0;
	}
	f->read = ck_read;
	f->write = ck_write;
	f->seek = ck_seek;
	f->close = ck_close;
	return f;
}
