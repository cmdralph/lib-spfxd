/* lib-spfxd — <sys/signalfd.h> */
#ifndef _SYS_SIGNALFD_H
#define _SYS_SIGNALFD_H
#include <features.h>
#include <stdint.h>
#include <signal.h>
#define SFD_NONBLOCK 04000
#define SFD_CLOEXEC 02000000
struct signalfd_siginfo {
	uint32_t ssi_signo;
	int32_t ssi_errno, ssi_code;
	uint32_t ssi_pid, ssi_uid;
	int32_t ssi_fd;
	uint32_t ssi_tid, ssi_band, ssi_overrun, ssi_trapno;
	int32_t ssi_status, ssi_int;
	uint64_t ssi_ptr, ssi_utime, ssi_stime, ssi_addr;
	uint16_t ssi_addr_lsb, __pad2;
	int32_t ssi_syscall;
	uint64_t ssi_call_addr;
	uint32_t ssi_arch;
	uint8_t __pad[28];
};
__SPFXD_BEGIN_DECLS
int signalfd(int, const sigset_t *, int);
__SPFXD_END_DECLS
#endif
