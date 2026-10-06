/* lib-spfxd — <sys/ipc.h> */
#ifndef _SYS_IPC_H
#define _SYS_IPC_H
#include <features.h>
#define __SPFXD_NEED_uid_t
#define __SPFXD_NEED_gid_t
#define __SPFXD_NEED_mode_t
#define __SPFXD_NEED_key_t
#include <bits/typedefs.h>

__SPFXD_BEGIN_DECLS

/* Kernel's ipc64_perm layout on x86-64 */
struct ipc_perm {
	key_t __key;
	uid_t uid;
	gid_t gid;
	uid_t cuid;
	gid_t cgid;
	unsigned short mode;
	unsigned short __pad1;
	unsigned short __seq;
	unsigned short __pad2;
	unsigned long __unused1, __unused2;
};

#define IPC_CREAT   01000
#define IPC_EXCL    02000
#define IPC_NOWAIT  04000
#define IPC_PRIVATE ((key_t)0)
#define IPC_RMID    0
#define IPC_SET     1
#define IPC_STAT    2
#define IPC_INFO    3

key_t ftok(const char *, int);

__SPFXD_END_DECLS
#endif
