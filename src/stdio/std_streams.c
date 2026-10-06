/*
 * lib-spfxd — the standard streams.
 *
 * stdin and stdout are fully buffered with static buffers; stdout switches
 * to line buffering on its first write if it refers to a terminal.  stderr
 * is unbuffered (each call writes immediately) as ISO C requires.
 */
#include "stdio_impl.h"

static unsigned char stdin_buf[BUFSIZ + UNGET];
static unsigned char stdout_buf[BUFSIZ + UNGET];

hidden FILE __stdin_FILE = {
	.flags = F_PERM | F_NOWR,
	.buf = stdin_buf + UNGET,
	.buf_size = BUFSIZ,
	.buf_cap = BUFSIZ,
	.fd = 0,
	.lbf = EOF,
	.read = __stdio_read,
	.seek = __stdio_seek,
	.close = __stdio_close,
};

hidden FILE __stdout_FILE = {
	.flags = F_PERM | F_NORD | F_TTYCHK,
	.buf = stdout_buf + UNGET,
	.buf_size = BUFSIZ,
	.buf_cap = BUFSIZ,
	.fd = 1,
	.lbf = EOF,
	.write = __stdio_write,
	.seek = __stdio_seek,
	.close = __stdio_close,
};

hidden FILE __stderr_FILE = {
	.flags = F_PERM | F_NORD,
	.buf = __stderr_FILE.small_buf + UNGET,
	.buf_size = 0,
	.fd = 2,
	.lbf = EOF,
	.write = __stdio_write,
	.seek = __stdio_seek,
	.close = __stdio_close,
};

FILE *const stdin = &__stdin_FILE;
FILE *const stdout = &__stdout_FILE;
FILE *const stderr = &__stderr_FILE;
