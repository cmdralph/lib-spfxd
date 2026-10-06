/* lib-spfxd — <utmpx.h>
 *
 * The record layout is that of the system's existing utmp/wtmp files on
 * x86-64 Linux (384-byte records, 32-bit time fields), so lib-spfxd
 * programs read and write the same database as everything else.  For
 * that reason ut_tv is a pair of 32-bit integers rather than a struct
 * timeval.
 */
#ifndef _UTMPX_H
#define _UTMPX_H
#include <features.h>
#define __SPFXD_NEED_pid_t
#define __SPFXD_NEED_intN_t
#include <bits/typedefs.h>

__SPFXD_BEGIN_DECLS

#define EMPTY         0
#define RUN_LVL       1
#define BOOT_TIME     2
#define NEW_TIME      3
#define OLD_TIME      4
#define INIT_PROCESS  5
#define LOGIN_PROCESS 6
#define USER_PROCESS  7
#define DEAD_PROCESS  8
#define ACCOUNTING    9

#define __UT_LINESIZE 32
#define __UT_NAMESIZE 32
#define __UT_HOSTSIZE 256

struct __exit_status {
	short e_termination;
	short e_exit;
};

struct utmpx {
	short ut_type;
	pid_t ut_pid;
	char ut_line[__UT_LINESIZE];
	char ut_id[4];
	char ut_user[__UT_NAMESIZE];
	char ut_host[__UT_HOSTSIZE];
	struct __exit_status ut_exit;
	int32_t ut_session;
	struct {
		int32_t tv_sec;
		int32_t tv_usec;
	} ut_tv;
	int32_t ut_addr_v6[4];
	char __unused[20];
};

#define UTMPX_FILE "/var/run/utmp"
#define WTMPX_FILE "/var/log/wtmp"

void endutxent(void);
struct utmpx *getutxent(void);
struct utmpx *getutxid(const struct utmpx *);
struct utmpx *getutxline(const struct utmpx *);
struct utmpx *pututxline(const struct utmpx *);
void setutxent(void);

#ifdef __SPFXD_GNU
int utmpxname(const char *);
void updwtmpx(const char *, const struct utmpx *);
#endif

__SPFXD_END_DECLS
#endif
