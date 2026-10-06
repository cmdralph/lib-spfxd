/* lib-spfxd — <sys/types.h> */
#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H
#include <features.h>

#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_ssize_t
#define __SPFXD_NEED_off_t
#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_uid_t
#define __SPFXD_NEED_gid_t
#define __SPFXD_NEED_id_t
#define __SPFXD_NEED_mode_t
#define __SPFXD_NEED_nlink_t
#define __SPFXD_NEED_ino_t
#define __SPFXD_NEED_dev_t
#define __SPFXD_NEED_blksize_t
#define __SPFXD_NEED_blkcnt_t
#define __SPFXD_NEED_fsblkcnt_t
#define __SPFXD_NEED_time_t
#define __SPFXD_NEED_clock_t
#define __SPFXD_NEED_clockid_t
#define __SPFXD_NEED_timer_t
#define __SPFXD_NEED_suseconds_t
#define __SPFXD_NEED_useconds_t
#define __SPFXD_NEED_key_t
#define __SPFXD_NEED_pthread_t
#define __SPFXD_NEED_pthread_types
#define __SPFXD_NEED_intN_t
#define __SPFXD_NEED_register_t
#if defined(__SPFXD_GNU)
#define __SPFXD_NEED_off64_t
#endif
#include <bits/typedefs.h>

#if defined(__SPFXD_BSD)
typedef unsigned char u_char;
typedef unsigned short u_short, ushort;
typedef unsigned int u_int, uint;
typedef unsigned long u_long, ulong;
typedef long long quad_t;
typedef unsigned long long u_quad_t;
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
typedef char *caddr_t;
typedef long loff_t;
#include <endian.h>
#include <sys/select.h>
#include <sys/sysmacros.h>
#endif

#endif
