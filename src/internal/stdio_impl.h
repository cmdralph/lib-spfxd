/*
 * lib-spfxd — stdio internals.
 *
 * A FILE is in one of three states:
 *   idle     rpos == rend == 0 and wpos == wend == 0
 *   reading  [rpos, rend) holds buffered input not yet consumed
 *   writing  [wbase, wpos) holds output not yet written; wend marks the
 *            end of the buffer.  An unbuffered stream keeps wend == wpos so
 *            every write takes the slow path and goes straight out.
 *
 * Hot single-character paths (getc/putc) are therefore a pointer compare
 * and a load/store.  All device I/O goes through four per-stream function
 * pointers, which lets the same machinery serve file descriptors, memory
 * streams, string formatting (sprintf/sscanf) and user cookies.
 *
 *   read(f, dst, n)   fill dst with up to n bytes; 0 = EOF or error
 *                     (the function sets F_EOF / F_ERR)
 *   write(f, s, n)    write the pending buffer [wbase, wpos) and then
 *                     s[0..n); returns how many bytes of s were written
 *                     (n on success) and resets the buffer
 *   seek(f, off, w)   reposition the underlying object; -1 on error
 *   close(f)          release the underlying object
 *
 * Every buffer is preceded by UNGET bytes of slack so ungetc can always
 * push back characters in front of rpos.
 */
#ifndef _SPFXD_STDIO_IMPL_H
#define _SPFXD_STDIO_IMPL_H

#include <stdio.h>
#include <wchar.h>
#include <sys/types.h>
#include "libc.h"

#define UNGET 8

#define F_PERM  0x0001   /* static FILE (stdin/stdout/stderr): never freed */
#define F_NORD  0x0004   /* not open for reading */
#define F_NOWR  0x0008   /* not open for writing */
#define F_EOF   0x0010
#define F_ERR   0x0020
#define F_SVB   0x0040   /* buffer supplied by the user (setvbuf) */
#define F_APP   0x0080   /* append mode */
#define F_TTYCHK 0x0100  /* decide line buffering on first write (isatty) */
#define F_ABUF  0x0200   /* buffer allocated by the library */
#define F_NOLOCK 0x0400  /* never locked (private temporary FILEs) */
#define F_STR   0x0800   /* the whole input is in memory (sscanf) */

#define MAYBE_WAITERS 0x40000000

struct __spfxd_file {
	unsigned flags;
	unsigned char *rpos, *rend;
	unsigned char *wpos, *wend;
	unsigned char *wbase;
	unsigned char *buf;
	size_t buf_size;            /* active buffer size (0 = unbuffered) */
	size_t buf_cap;             /* capacity of buf */
	int fd;
	int lbf;                    /* '\n' when line buffered, EOF otherwise */
	int mode;                   /* orientation: 0 none, <0 byte, >0 wide */
	volatile int lock;          /* owner tid (| MAYBE_WAITERS), 0 when free */
	int lockcount;              /* flockfile recursion depth */
	size_t (*read)(FILE *, unsigned char *, size_t);
	size_t (*write)(FILE *, const unsigned char *, size_t);
	off_t (*seek)(FILE *, off_t, int);
	int (*close)(FILE *);
	void *cookie;
	FILE *prev, *next;          /* list of open streams */
	mbstate_t mbs;
	int pipe_pid;               /* popen child */
	char *getln_buf;            /* fgetln storage */
	unsigned char small_buf[UNGET + 1];
};

/* fd-backed stream operations */
hidden size_t __stdio_read(FILE *, unsigned char *, size_t);
hidden size_t __stdio_write(FILE *, const unsigned char *, size_t);
hidden off_t __stdio_seek(FILE *, off_t, int);
hidden int __stdio_close(FILE *);

/* buffer state transitions */
hidden int __toread(FILE *);
hidden int __towrite(FILE *);
hidden int __uflow(FILE *);
hidden void __stdin_refill_hook(FILE *);
hidden int __overflow(FILE *, int);
hidden size_t __fwritex(const unsigned char *, size_t, FILE *);
hidden int __fflush_unlocked(FILE *);
hidden off_t __ftello_unlocked(FILE *);
hidden int __fseeko_unlocked(FILE *, off_t, int);

/* open-file list */
hidden FILE **__ofl_lock(void);
hidden void __ofl_unlock(void);
hidden FILE *__ofl_add(FILE *);

/* stream creation helpers */
hidden int __fmodeflags(const char *);
hidden FILE *__fdopen_flags(int fd, int flags);

/* locking */
hidden int __lockfile(FILE *);
hidden void __unlockfile(FILE *);
#define FLOCK(f) int __need_unlock = __libc.threaded ? __lockfile(f) : 0
#define FUNLOCK(f) do { if (__need_unlock) __unlockfile(f); } while (0)

/* formatting engines */
hidden int __vfprintf_core(FILE *, const char *, __builtin_va_list);
hidden int __vfscanf_core(FILE *, const char *, __builtin_va_list);
hidden int __vfwprintf_core(FILE *, const wchar_t *, __builtin_va_list);
hidden int __vfwscanf_core(FILE *, const wchar_t *, __builtin_va_list);

/* character I/O fast paths (stream already locked) */
static __inline int getc_fast(FILE *f)
{
	return f->rpos != f->rend ? *f->rpos++ : __uflow(f);
}

static __inline int putc_fast(int c, FILE *f)
{
	unsigned char ch = (unsigned char)c;
	if (ch != f->lbf && f->wpos != f->wend) return *f->wpos++ = ch;
	return __overflow(f, ch);
}

/* Initialize a private string-backed FILE (sprintf, sscanf, ...). */
hidden void __string_file_init(FILE *, unsigned char *buf, size_t size);

extern hidden FILE __stdin_FILE, __stdout_FILE, __stderr_FILE;

/* printf flag bits (shared by the integer and floating-point formatters) */
#define FL_ALT    (1U << 0)   /* # */
#define FL_ZERO   (1U << 1)   /* 0 */
#define FL_LEFT   (1U << 2)   /* - */
#define FL_SPACE  (1U << 3)   /* space */
#define FL_PLUS   (1U << 4)   /* + */
#define FL_GROUP  (1U << 5)   /* ' */
#define FL_LDBL   (1U << 6)   /* argument was long double (for %a layout) */

/* floating point conversion shared by printf, scanf, strtod, ecvt */
hidden int __fmt_fp(FILE *, long double, int, int, unsigned, int);
hidden long double __strtold_internal(const char *, char **, int prec);
hidden long double __scan_float(FILE *, int prec, int *ok);

#endif
