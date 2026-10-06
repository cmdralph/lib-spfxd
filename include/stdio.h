/* lib-spfxd — <stdio.h> */
#ifndef _STDIO_H
#define _STDIO_H
#include <features.h>

#define __SPFXD_NEED_FILE
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED___va_list
#if defined(__SPFXD_POSIX)
#define __SPFXD_NEED_ssize_t
#define __SPFXD_NEED_off_t
#define __SPFXD_NEED_va_list
#endif
#include <bits/typedefs.h>

#ifdef __cplusplus
# define NULL 0L
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
#define BUFSIZ 8192
#define FILENAME_MAX 4096
#define FOPEN_MAX 1000
#define TMP_MAX 238328
#define L_tmpnam 20
#if defined(__SPFXD_POSIX)
#define L_ctermid 20
#define P_tmpdir "/tmp"
#endif
#if defined(__SPFXD_GNU)
#define L_cuserid 20
#endif

typedef struct { long long __pos; unsigned int __mb[2]; } fpos_t;

__SPFXD_BEGIN_DECLS

extern FILE *const stdin;
extern FILE *const stdout;
extern FILE *const stderr;
#define stdin  (stdin)
#define stdout (stdout)
#define stderr (stderr)

FILE *fopen(const char *__restrict, const char *__restrict);
FILE *freopen(const char *__restrict, const char *__restrict, FILE *__restrict);
int fclose(FILE *);
int remove(const char *);
int rename(const char *, const char *);
int feof(FILE *);
int ferror(FILE *);
int fflush(FILE *);
void clearerr(FILE *);
int fseek(FILE *, long, int);
long ftell(FILE *);
void rewind(FILE *);
int fgetpos(FILE *__restrict, fpos_t *__restrict);
int fsetpos(FILE *, const fpos_t *);
size_t fread(void *__restrict, size_t, size_t, FILE *__restrict);
size_t fwrite(const void *__restrict, size_t, size_t, FILE *__restrict);
int fgetc(FILE *);
int getc(FILE *);
int getchar(void);
int ungetc(int, FILE *);
int fputc(int, FILE *);
int putc(int, FILE *);
int putchar(int);
char *fgets(char *__restrict, int, FILE *__restrict);
int fputs(const char *__restrict, FILE *__restrict);
int puts(const char *);

int printf(const char *__restrict, ...) __spfxd_printf(1, 2);
int fprintf(FILE *__restrict, const char *__restrict, ...) __spfxd_printf(2, 3);
int sprintf(char *__restrict, const char *__restrict, ...) __spfxd_printf(2, 3);
int snprintf(char *__restrict, size_t, const char *__restrict, ...) __spfxd_printf(3, 4);
int vprintf(const char *__restrict, __spfxd_va_list) __spfxd_printf(1, 0);
int vfprintf(FILE *__restrict, const char *__restrict, __spfxd_va_list) __spfxd_printf(2, 0);
int vsprintf(char *__restrict, const char *__restrict, __spfxd_va_list) __spfxd_printf(2, 0);
int vsnprintf(char *__restrict, size_t, const char *__restrict, __spfxd_va_list) __spfxd_printf(3, 0);

int scanf(const char *__restrict, ...) __spfxd_scanf(1, 2);
int fscanf(FILE *__restrict, const char *__restrict, ...) __spfxd_scanf(2, 3);
int sscanf(const char *__restrict, const char *__restrict, ...) __spfxd_scanf(2, 3);
int vscanf(const char *__restrict, __spfxd_va_list) __spfxd_scanf(1, 0);
int vfscanf(FILE *__restrict, const char *__restrict, __spfxd_va_list) __spfxd_scanf(2, 0);
int vsscanf(const char *__restrict, const char *__restrict, __spfxd_va_list) __spfxd_scanf(2, 0);

void perror(const char *);
int setvbuf(FILE *__restrict, char *__restrict, int, size_t);
void setbuf(FILE *__restrict, char *__restrict);
char *tmpnam(char *);
FILE *tmpfile(void);

#if defined(__SPFXD_POSIX)
FILE *fdopen(int, const char *);
int fileno(FILE *);
#ifdef __SPFXD_BSD
int fpurge(FILE *);
#endif
FILE *fmemopen(void *__restrict, size_t, const char *__restrict);
FILE *open_memstream(char **, size_t *);
FILE *popen(const char *, const char *);
int pclose(FILE *);
int fseeko(FILE *, off_t, int);
off_t ftello(FILE *);
int dprintf(int, const char *__restrict, ...) __spfxd_printf(2, 3);
int vdprintf(int, const char *__restrict, __spfxd_va_list) __spfxd_printf(2, 0);
void flockfile(FILE *);
int ftrylockfile(FILE *);
void funlockfile(FILE *);
int getc_unlocked(FILE *);
int getchar_unlocked(void);
int putc_unlocked(int, FILE *);
int putchar_unlocked(int);
ssize_t getdelim(char **__restrict, size_t *__restrict, int, FILE *__restrict);
ssize_t getline(char **__restrict, size_t *__restrict, FILE *__restrict);
int renameat(int, const char *, int, const char *);
char *ctermid(char *);
#endif

#if defined(__SPFXD_XSI)
char *tempnam(const char *, const char *);
#endif

#if defined(__SPFXD_BSD)
void setlinebuf(FILE *);
void setbuffer(FILE *, char *, size_t);
int fgetc_unlocked(FILE *);
int fputc_unlocked(int, FILE *);
int fflush_unlocked(FILE *);
size_t fread_unlocked(void *, size_t, size_t, FILE *);
size_t fwrite_unlocked(const void *, size_t, size_t, FILE *);
void clearerr_unlocked(FILE *);
int feof_unlocked(FILE *);
int ferror_unlocked(FILE *);
int fileno_unlocked(FILE *);
int getw(FILE *);
int putw(int, FILE *);
char *fgetln(FILE *, size_t *);
int asprintf(char **, const char *, ...) __spfxd_printf(2, 3);
int vasprintf(char **, const char *, __spfxd_va_list) __spfxd_printf(2, 0);
#endif

#if defined(__SPFXD_GNU)
char *fgets_unlocked(char *, int, FILE *);
int fputs_unlocked(const char *, FILE *);
typedef ssize_t (cookie_read_function_t)(void *, char *, size_t);
typedef ssize_t (cookie_write_function_t)(void *, const char *, size_t);
typedef int (cookie_seek_function_t)(void *, off_t *, int);
typedef int (cookie_close_function_t)(void *);
typedef struct {
	cookie_read_function_t *read;
	cookie_write_function_t *write;
	cookie_seek_function_t *seek;
	cookie_close_function_t *close;
} cookie_io_functions_t;
FILE *fopencookie(void *, const char *, cookie_io_functions_t);
#endif

__SPFXD_END_DECLS
#endif
