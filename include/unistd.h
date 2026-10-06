/* lib-spfxd — <unistd.h> */
#ifndef _UNISTD_H
#define _UNISTD_H
#include <features.h>

#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_ssize_t
#define __SPFXD_NEED_off_t
#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_uid_t
#define __SPFXD_NEED_gid_t
#define __SPFXD_NEED_useconds_t
#define __SPFXD_NEED_intptr_t
#if defined(__SPFXD_GNU)
#define __SPFXD_NEED_off64_t
#endif
#include <bits/typedefs.h>

#ifdef __cplusplus
# define NULL 0L
#else
# ifndef NULL
#  define NULL ((void *)0)
# endif
#endif

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2
#define SEEK_DATA 3
#define SEEK_HOLE 4

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#define F_ULOCK 0
#define F_LOCK  1
#define F_TLOCK 2
#define F_TEST  3

#define _POSIX_VERSION    200809L
#define _POSIX2_VERSION   200809L
#define _XOPEN_VERSION    700
#define _POSIX_ADVISORY_INFO    200809L
#define _POSIX_BARRIERS         200809L
#define _POSIX_CLOCK_SELECTION  200809L
#define _POSIX_CPUTIME          200809L
#define _POSIX_FSYNC            200809L
#define _POSIX_IPV6             200809L
#define _POSIX_JOB_CONTROL      1
#define _POSIX_MAPPED_FILES     200809L
#define _POSIX_MEMLOCK          200809L
#define _POSIX_MEMLOCK_RANGE    200809L
#define _POSIX_MEMORY_PROTECTION 200809L
#define _POSIX_MONOTONIC_CLOCK  200809L
#define _POSIX_NO_TRUNC         1
#define _POSIX_RAW_SOCKETS      200809L
#define _POSIX_READER_WRITER_LOCKS 200809L
#define _POSIX_REALTIME_SIGNALS 200809L
#define _POSIX_REGEXP           1
#define _POSIX_SAVED_IDS        1
#define _POSIX_SEMAPHORES       200809L
#define _POSIX_SHELL            1
#define _POSIX_SPAWN            200809L
#define _POSIX_SPIN_LOCKS       200809L
#define _POSIX_SYNCHRONIZED_IO  200809L
#define _POSIX_THREAD_ATTR_STACKADDR 200809L
#define _POSIX_THREAD_ATTR_STACKSIZE 200809L
#define _POSIX_THREAD_CPUTIME   200809L
#define _POSIX_THREAD_SAFE_FUNCTIONS 200809L
#define _POSIX_THREADS          200809L
#define _POSIX_TIMEOUTS         200809L
#define _POSIX_TIMERS           200809L
#define _POSIX_VDISABLE         0
#define _POSIX2_C_BIND          200809L

__SPFXD_BEGIN_DECLS

int access(const char *, int);
int faccessat(int, const char *, int, int);
unsigned alarm(unsigned);
int chdir(const char *);
int fchdir(int);
int chown(const char *, uid_t, gid_t);
int fchown(int, uid_t, gid_t);
int lchown(const char *, uid_t, gid_t);
int fchownat(int, const char *, uid_t, gid_t, int);
int close(int);
size_t confstr(int, char *, size_t);
int dup(int);
int dup2(int, int);
int execl(const char *, const char *, ...);
int execle(const char *, const char *, ...);
int execlp(const char *, const char *, ...);
int execv(const char *, char *const[]);
int execve(const char *, char *const[], char *const[]);
int execvp(const char *, char *const[]);
int fexecve(int, char *const[], char *const[]);
__spfxd_noreturn void _exit(int);
long fpathconf(int, int);
long pathconf(const char *, int);
int fsync(int);
int fdatasync(int);
pid_t fork(void);
pid_t _Fork(void);
int ftruncate(int, off_t);
int truncate(const char *, off_t);
char *getcwd(char *, size_t);
gid_t getegid(void);
uid_t geteuid(void);
gid_t getgid(void);
uid_t getuid(void);
int getgroups(int, gid_t[]);
int gethostname(char *, size_t);
char *getlogin(void);
int getlogin_r(char *, size_t);
int getopt(int, char *const[], const char *);
extern char *optarg;
extern int optind, opterr, optopt;
pid_t getpgid(pid_t);
pid_t getpgrp(void);
pid_t getpid(void);
pid_t getppid(void);
pid_t getsid(pid_t);
int isatty(int);
int link(const char *, const char *);
int linkat(int, const char *, int, const char *, int);
off_t lseek(int, off_t, int);
int pause(void);
int pipe(int[2]);
ssize_t pread(int, void *, size_t, off_t);
ssize_t pwrite(int, const void *, size_t, off_t);
ssize_t read(int, void *, size_t);
ssize_t readlink(const char *__restrict, char *__restrict, size_t);
ssize_t readlinkat(int, const char *__restrict, char *__restrict, size_t);
int rmdir(const char *);
int setegid(gid_t);
int seteuid(uid_t);
int setgid(gid_t);
int setpgid(pid_t, pid_t);
int setregid(gid_t, gid_t);
int setreuid(uid_t, uid_t);
pid_t setsid(void);
int setuid(uid_t);
unsigned sleep(unsigned);
int symlink(const char *, const char *);
int symlinkat(const char *, int, const char *);
long sysconf(int);
pid_t tcgetpgrp(int);
int tcsetpgrp(int, pid_t);
char *ttyname(int);
int ttyname_r(int, char *, size_t);
int unlink(const char *);
int unlinkat(int, const char *, int);
ssize_t write(int, const void *, size_t);
extern char **environ;

#if defined(__SPFXD_XSI) || defined(__SPFXD_BSD)
int lockf(int, int, off_t);
long gethostid(void);
int nice(int);
void sync(void);
pid_t setpgrp(void);
void swab(const void *__restrict, void *__restrict, ssize_t);
char *crypt(const char *, const char *);
int usleep(useconds_t);
pid_t vfork(void);
int brk(void *);
void *sbrk(intptr_t);
int getpagesize(void);
int getdtablesize(void);
int sethostname(const char *, size_t);
int getdomainname(char *, size_t);
int setdomainname(const char *, size_t);
int setgroups(size_t, const gid_t *);
int chroot(const char *);
int daemon(int, int);
char *getpass(const char *);
int acct(const char *);
int vhangup(void);
int getentropy(void *, size_t);
int getresuid(uid_t *, uid_t *, uid_t *);
int getresgid(gid_t *, gid_t *, gid_t *);
int setresuid(uid_t, uid_t, uid_t);
int setresgid(gid_t, gid_t, gid_t);
#endif

#if defined(__SPFXD_GNU)
int pipe2(int[2], int);
int dup3(int, int, int);
int execvpe(const char *, char *const[], char *const[]);
int euidaccess(const char *, int);
int eaccess(const char *, int);
pid_t gettid(void);
char *get_current_dir_name(void);
int syncfs(int);
ssize_t copy_file_range(int, off_t *, int, off_t *, size_t, unsigned);
#endif

#if defined(__SPFXD_BSD) || defined(__SPFXD_GNU)
long syscall(long, ...);
int issetugid(void);
#endif

/* sysconf names */
#define _SC_ARG_MAX 0
#define _SC_CHILD_MAX 1
#define _SC_CLK_TCK 2
#define _SC_NGROUPS_MAX 3
#define _SC_OPEN_MAX 4
#define _SC_STREAM_MAX 5
#define _SC_TZNAME_MAX 6
#define _SC_JOB_CONTROL 7
#define _SC_SAVED_IDS 8
#define _SC_REALTIME_SIGNALS 9
#define _SC_VERSION 29
#define _SC_PAGESIZE 30
#define _SC_PAGE_SIZE 30
#define _SC_RTSIG_MAX 31
#define _SC_SEM_NSEMS_MAX 32
#define _SC_SEM_VALUE_MAX 33
#define _SC_SIGQUEUE_MAX 34
#define _SC_TIMER_MAX 35
#define _SC_BC_BASE_MAX 36
#define _SC_BC_DIM_MAX 37
#define _SC_BC_SCALE_MAX 38
#define _SC_BC_STRING_MAX 39
#define _SC_COLL_WEIGHTS_MAX 40
#define _SC_EXPR_NEST_MAX 42
#define _SC_LINE_MAX 43
#define _SC_RE_DUP_MAX 44
#define _SC_2_VERSION 46
#define _SC_IOV_MAX 60
#define _SC_THREADS 67
#define _SC_THREAD_SAFE_FUNCTIONS 68
#define _SC_GETGR_R_SIZE_MAX 69
#define _SC_GETPW_R_SIZE_MAX 70
#define _SC_LOGIN_NAME_MAX 71
#define _SC_TTY_NAME_MAX 72
#define _SC_THREAD_DESTRUCTOR_ITERATIONS 73
#define _SC_THREAD_KEYS_MAX 74
#define _SC_THREAD_STACK_MIN 75
#define _SC_THREAD_THREADS_MAX 76
#define _SC_NPROCESSORS_CONF 83
#define _SC_NPROCESSORS_ONLN 84
#define _SC_PHYS_PAGES 85
#define _SC_AVPHYS_PAGES 86
#define _SC_ATEXIT_MAX 87
#define _SC_PASS_MAX 88
#define _SC_XOPEN_VERSION 89
#define _SC_MONOTONIC_CLOCK 149
#define _SC_HOST_NAME_MAX 180
#define _SC_SYMLOOP_MAX 173
#define _SC_LEVEL1_DCACHE_LINESIZE 190

/* pathconf names */
#define _PC_LINK_MAX 0
#define _PC_MAX_CANON 1
#define _PC_MAX_INPUT 2
#define _PC_NAME_MAX 3
#define _PC_PATH_MAX 4
#define _PC_PIPE_BUF 5
#define _PC_CHOWN_RESTRICTED 6
#define _PC_NO_TRUNC 7
#define _PC_VDISABLE 8
#define _PC_SYNC_IO 9
#define _PC_ASYNC_IO 10
#define _PC_PRIO_IO 11
#define _PC_FILESIZEBITS 13
#define _PC_SYMLINK_MAX 19

#define _CS_PATH 0

__SPFXD_END_DECLS
#endif
