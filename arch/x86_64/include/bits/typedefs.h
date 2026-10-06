/*
 * lib-spfxd — bits/typedefs.h (x86-64)
 *
 * Every public type whose definition depends on the target lives here exactly
 * once.  A public header requests the types it is allowed to expose by
 * defining __SPFXD_NEED_<type> before including this file; each type is then
 * defined at most once per translation unit no matter how many headers ask
 * for it.  This keeps the namespace of each standard header clean (<string.h>
 * must not leak off_t, for instance) without duplicating definitions.
 *
 * This header intentionally has no include guard.
 */

/* ---- fundamental C types ------------------------------------------------ */
#if defined(__SPFXD_NEED_size_t) && !defined(__SPFXD_DEF_size_t)
#define __SPFXD_DEF_size_t
typedef unsigned long size_t;
#endif
#if defined(__SPFXD_NEED_ssize_t) && !defined(__SPFXD_DEF_ssize_t)
#define __SPFXD_DEF_ssize_t
typedef long ssize_t;
#endif
#if defined(__SPFXD_NEED_ptrdiff_t) && !defined(__SPFXD_DEF_ptrdiff_t)
#define __SPFXD_DEF_ptrdiff_t
typedef long ptrdiff_t;
#endif
#if defined(__SPFXD_NEED_wchar_t) && !defined(__SPFXD_DEF_wchar_t) && !defined(__cplusplus)
#define __SPFXD_DEF_wchar_t
typedef int wchar_t;
#endif
#if defined(__SPFXD_NEED_wint_t) && !defined(__SPFXD_DEF_wint_t)
#define __SPFXD_DEF_wint_t
typedef unsigned int wint_t;
#endif
#if defined(__SPFXD_NEED_wctype_t) && !defined(__SPFXD_DEF_wctype_t)
#define __SPFXD_DEF_wctype_t
typedef unsigned long wctype_t;
#endif
#if defined(__SPFXD_NEED_wctrans_t) && !defined(__SPFXD_DEF_wctrans_t)
#define __SPFXD_DEF_wctrans_t
typedef const int *wctrans_t;
#endif
#if defined(__SPFXD_NEED_max_align_t) && !defined(__SPFXD_DEF_max_align_t)
#define __SPFXD_DEF_max_align_t
typedef struct {
	long long __ll __attribute__((__aligned__(__alignof__(long long))));
	long double __ld __attribute__((__aligned__(__alignof__(long double))));
} max_align_t;
#endif

/* ---- exact width integers ---------------------------------------------- */
#if defined(__SPFXD_NEED_intN_t) && !defined(__SPFXD_DEF_intN_t)
#define __SPFXD_DEF_intN_t
typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long               int64_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long      uint64_t;
#endif
#if defined(__SPFXD_NEED_intptr_t) && !defined(__SPFXD_DEF_intptr_t)
#define __SPFXD_DEF_intptr_t
typedef long               intptr_t;
#endif
#if defined(__SPFXD_NEED_uintptr_t) && !defined(__SPFXD_DEF_uintptr_t)
#define __SPFXD_DEF_uintptr_t
typedef unsigned long      uintptr_t;
#endif
#if defined(__SPFXD_NEED_intmax_t) && !defined(__SPFXD_DEF_intmax_t)
#define __SPFXD_DEF_intmax_t
typedef long               intmax_t;
typedef unsigned long      uintmax_t;
#endif

/* ---- POSIX system types ------------------------------------------------- */
#if defined(__SPFXD_NEED_off_t) && !defined(__SPFXD_DEF_off_t)
#define __SPFXD_DEF_off_t
typedef long off_t;
#endif
#if defined(__SPFXD_NEED_off64_t) && !defined(__SPFXD_DEF_off64_t)
#define __SPFXD_DEF_off64_t
typedef long off64_t;
#endif
#if defined(__SPFXD_NEED_pid_t) && !defined(__SPFXD_DEF_pid_t)
#define __SPFXD_DEF_pid_t
typedef int pid_t;
#endif
#if defined(__SPFXD_NEED_uid_t) && !defined(__SPFXD_DEF_uid_t)
#define __SPFXD_DEF_uid_t
typedef unsigned int uid_t;
#endif
#if defined(__SPFXD_NEED_gid_t) && !defined(__SPFXD_DEF_gid_t)
#define __SPFXD_DEF_gid_t
typedef unsigned int gid_t;
#endif
#if defined(__SPFXD_NEED_id_t) && !defined(__SPFXD_DEF_id_t)
#define __SPFXD_DEF_id_t
typedef unsigned int id_t;
#endif
#if defined(__SPFXD_NEED_mode_t) && !defined(__SPFXD_DEF_mode_t)
#define __SPFXD_DEF_mode_t
typedef unsigned int mode_t;
#endif
#if defined(__SPFXD_NEED_nlink_t) && !defined(__SPFXD_DEF_nlink_t)
#define __SPFXD_DEF_nlink_t
typedef unsigned long nlink_t;
#endif
#if defined(__SPFXD_NEED_ino_t) && !defined(__SPFXD_DEF_ino_t)
#define __SPFXD_DEF_ino_t
typedef unsigned long ino_t;
#endif
#if defined(__SPFXD_NEED_dev_t) && !defined(__SPFXD_DEF_dev_t)
#define __SPFXD_DEF_dev_t
typedef unsigned long dev_t;
#endif
#if defined(__SPFXD_NEED_blksize_t) && !defined(__SPFXD_DEF_blksize_t)
#define __SPFXD_DEF_blksize_t
typedef long blksize_t;
#endif
#if defined(__SPFXD_NEED_blkcnt_t) && !defined(__SPFXD_DEF_blkcnt_t)
#define __SPFXD_DEF_blkcnt_t
typedef long blkcnt_t;
#endif
#if defined(__SPFXD_NEED_fsblkcnt_t) && !defined(__SPFXD_DEF_fsblkcnt_t)
#define __SPFXD_DEF_fsblkcnt_t
typedef unsigned long fsblkcnt_t;
typedef unsigned long fsfilcnt_t;
#endif
#if defined(__SPFXD_NEED_time_t) && !defined(__SPFXD_DEF_time_t)
#define __SPFXD_DEF_time_t
typedef long time_t;
#endif
#if defined(__SPFXD_NEED_suseconds_t) && !defined(__SPFXD_DEF_suseconds_t)
#define __SPFXD_DEF_suseconds_t
typedef long suseconds_t;
#endif
#if defined(__SPFXD_NEED_useconds_t) && !defined(__SPFXD_DEF_useconds_t)
#define __SPFXD_DEF_useconds_t
typedef unsigned int useconds_t;
#endif
#if defined(__SPFXD_NEED_clock_t) && !defined(__SPFXD_DEF_clock_t)
#define __SPFXD_DEF_clock_t
typedef long clock_t;
#endif
#if defined(__SPFXD_NEED_clockid_t) && !defined(__SPFXD_DEF_clockid_t)
#define __SPFXD_DEF_clockid_t
typedef int clockid_t;
#endif
#if defined(__SPFXD_NEED_timer_t) && !defined(__SPFXD_DEF_timer_t)
#define __SPFXD_DEF_timer_t
typedef void *timer_t;
#endif
#if defined(__SPFXD_NEED_key_t) && !defined(__SPFXD_DEF_key_t)
#define __SPFXD_DEF_key_t
typedef int key_t;
#endif
#if defined(__SPFXD_NEED_socklen_t) && !defined(__SPFXD_DEF_socklen_t)
#define __SPFXD_DEF_socklen_t
typedef unsigned int socklen_t;
#endif
#if defined(__SPFXD_NEED_sa_family_t) && !defined(__SPFXD_DEF_sa_family_t)
#define __SPFXD_DEF_sa_family_t
typedef unsigned short sa_family_t;
#endif
#if defined(__SPFXD_NEED_register_t) && !defined(__SPFXD_DEF_register_t)
#define __SPFXD_DEF_register_t
typedef long register_t;
#endif

/* ---- structures --------------------------------------------------------- */
#if defined(__SPFXD_NEED_struct_timespec) && !defined(__SPFXD_DEF_struct_timespec)
#define __SPFXD_DEF_struct_timespec
struct timespec { long tv_sec; long tv_nsec; };
#endif
#if defined(__SPFXD_NEED_struct_timeval) && !defined(__SPFXD_DEF_struct_timeval)
#define __SPFXD_DEF_struct_timeval
struct timeval { long tv_sec; long tv_usec; };
#endif
#if defined(__SPFXD_NEED_struct_itimerspec) && !defined(__SPFXD_DEF_struct_itimerspec)
#define __SPFXD_DEF_struct_itimerspec
struct itimerspec { struct timespec it_interval, it_value; };
#endif
#if defined(__SPFXD_NEED_struct_iovec) && !defined(__SPFXD_DEF_struct_iovec)
#define __SPFXD_DEF_struct_iovec
struct iovec { void *iov_base; unsigned long iov_len; };
#endif

/* ---- varargs ------------------------------------------------------------ */
#if defined(__SPFXD_NEED_va_list) && !defined(__SPFXD_DEF_va_list)
#define __SPFXD_DEF_va_list
typedef __builtin_va_list va_list;
#endif
#if defined(__SPFXD_NEED___va_list) && !defined(__SPFXD_DEF___va_list)
#define __SPFXD_DEF___va_list
typedef __builtin_va_list __spfxd_va_list;
#endif

/* ---- library objects ---------------------------------------------------- */
#if defined(__SPFXD_NEED_FILE) && !defined(__SPFXD_DEF_FILE)
#define __SPFXD_DEF_FILE
typedef struct __spfxd_file FILE;
#endif
#if defined(__SPFXD_NEED_locale_t) && !defined(__SPFXD_DEF_locale_t)
#define __SPFXD_DEF_locale_t
typedef struct __spfxd_locale *locale_t;
#endif
#if defined(__SPFXD_NEED_mbstate_t) && !defined(__SPFXD_DEF_mbstate_t)
#define __SPFXD_DEF_mbstate_t
typedef struct { unsigned int __pending; unsigned int __count; } mbstate_t;
#endif
#if defined(__SPFXD_NEED_sigset_t) && !defined(__SPFXD_DEF_sigset_t)
#define __SPFXD_DEF_sigset_t
/* 1024 signal bits, the conventional user-space size; the kernel only reads
 * the first 64 (see _NSIG). */
typedef struct { unsigned long __bits[16]; } sigset_t;
#endif

/* ---- threads ------------------------------------------------------------ */
#if defined(__SPFXD_NEED_pthread_t) && !defined(__SPFXD_DEF_pthread_t)
#define __SPFXD_DEF_pthread_t
typedef unsigned long pthread_t;
#endif
#if defined(__SPFXD_NEED_pthread_types) && !defined(__SPFXD_DEF_pthread_types)
#define __SPFXD_DEF_pthread_types
typedef unsigned int pthread_key_t;
typedef int pthread_once_t;
typedef volatile int pthread_spinlock_t;
typedef struct { unsigned int __attr; } pthread_mutexattr_t;
typedef struct { unsigned int __attr; } pthread_condattr_t;
typedef struct { unsigned int __attr; } pthread_rwlockattr_t;
typedef struct { unsigned int __attr; } pthread_barrierattr_t;
typedef struct {
	unsigned long __stacksize, __guardsize;
	void *__stackaddr;
	int __detach, __sched_policy, __sched_prio, __inherit, __scope;
	int __pad[3];
} pthread_attr_t;
/* lock word, owner tid, recursion count, type/flags.  40 bytes total. */
typedef struct {
	volatile int __lock;
	int __owner;
	int __count;
	int __type;
	int __waiters;
	int __pad[5];
} pthread_mutex_t;
typedef struct {
	volatile unsigned int __seq;
	volatile int __waiters;
	int __clock;
	int __shared;
	int __pad[8];
} pthread_cond_t;
typedef struct {
	volatile int __lock;
	volatile int __waiters;
	int __shared;
	int __writer;
	int __pad[10];
} pthread_rwlock_t;
typedef struct {
	volatile int __lock;
	volatile unsigned int __gen;
	int __count;
	int __limit;
	int __shared;
	int __pad[3];
} pthread_barrier_t;
#endif
