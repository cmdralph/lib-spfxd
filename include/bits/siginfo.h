/* lib-spfxd — bits/siginfo.h: signal data types shared by <signal.h>,
 * <sys/wait.h>, <ucontext.h> and <sys/signalfd.h> (not feature gated). */
#ifndef _BITS_SIGINFO_H
#define _BITS_SIGINFO_H

#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_uid_t
#define __SPFXD_NEED_clock_t
#define __SPFXD_NEED_sigset_t
#define __SPFXD_NEED_pthread_types
#include <bits/typedefs.h>

union sigval { int sival_int; void *sival_ptr; };

typedef struct {
	int si_signo, si_errno, si_code;
	union {
		char __pad[128 - 2 * sizeof(int) - sizeof(long)];
		struct {
			union {
				struct { pid_t si_pid; uid_t si_uid; } __piduid;
				struct { int si_timerid; int si_overrun; } __timer;
			} __first;
			union {
				union sigval si_value;
				struct { int si_status; clock_t si_utime, si_stime; } __sigchld;
			} __second;
		} __si_common;
		struct {
			void *si_addr;
			short si_addr_lsb;
			union {
				struct { void *si_lower; void *si_upper; } __addr_bnd;
				unsigned si_pkey;
			} __first;
		} __sigfault;
		struct { long si_band; int si_fd; } __sigpoll;
		struct { void *si_call_addr; int si_syscall; unsigned si_arch; } __sigsys;
	} __si_fields;
} siginfo_t;
#define si_pid       __si_fields.__si_common.__first.__piduid.si_pid
#define si_uid       __si_fields.__si_common.__first.__piduid.si_uid
#define si_status    __si_fields.__si_common.__second.__sigchld.si_status
#define si_utime     __si_fields.__si_common.__second.__sigchld.si_utime
#define si_stime     __si_fields.__si_common.__second.__sigchld.si_stime
#define si_value     __si_fields.__si_common.__second.si_value
#define si_int       si_value.sival_int
#define si_ptr       si_value.sival_ptr
#define si_timerid   __si_fields.__si_common.__first.__timer.si_timerid
#define si_overrun   __si_fields.__si_common.__first.__timer.si_overrun
#define si_addr      __si_fields.__sigfault.si_addr
#define si_addr_lsb  __si_fields.__sigfault.si_addr_lsb
#define si_lower     __si_fields.__sigfault.__first.__addr_bnd.si_lower
#define si_upper     __si_fields.__sigfault.__first.__addr_bnd.si_upper
#define si_pkey      __si_fields.__sigfault.__first.si_pkey
#define si_band      __si_fields.__sigpoll.si_band
#define si_fd        __si_fields.__sigpoll.si_fd
#define si_call_addr __si_fields.__sigsys.si_call_addr
#define si_syscall   __si_fields.__sigsys.si_syscall
#define si_arch      __si_fields.__sigsys.si_arch


typedef struct sigaltstack {
	void *ss_sp;
	int ss_flags;
	size_t ss_size;
} stack_t;

#define SIGEV_SIGNAL    0
#define SIGEV_NONE      1
#define SIGEV_THREAD    2
#define SIGEV_THREAD_ID 4

struct sigevent {
	union sigval sigev_value;
	int sigev_signo;
	int sigev_notify;
	union {
		char __pad[64 - 2 * sizeof(int) - sizeof(union sigval)];
		pid_t sigev_notify_thread_id;
		struct {
			void (*sigev_notify_function)(union sigval);
			pthread_attr_t *sigev_notify_attributes;
		} __sev_thread;
	} __sev_fields;
};
#define sigev_notify_thread_id   __sev_fields.sigev_notify_thread_id
#define sigev_notify_function    __sev_fields.__sev_thread.sigev_notify_function
#define sigev_notify_attributes  __sev_fields.__sev_thread.sigev_notify_attributes

#include <bits/signal.h>

#endif
