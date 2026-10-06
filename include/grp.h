/* lib-spfxd — <grp.h> (backed by /etc/group) */
#ifndef _GRP_H
#define _GRP_H
#include <features.h>
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_gid_t
#define __SPFXD_NEED_FILE
#include <bits/typedefs.h>
struct group {
	char *gr_name;
	char *gr_passwd;
	gid_t gr_gid;
	char **gr_mem;
};
__SPFXD_BEGIN_DECLS
struct group *getgrgid(gid_t);
struct group *getgrnam(const char *);
int getgrgid_r(gid_t, struct group *, char *, size_t, struct group **);
int getgrnam_r(const char *, struct group *, char *, size_t, struct group **);
#if defined(__SPFXD_XSI)
struct group *getgrent(void);
void endgrent(void);
void setgrent(void);
#endif
#if defined(__SPFXD_BSD)
int getgrouplist(const char *, gid_t, gid_t *, int *);
int initgroups(const char *, gid_t);
#endif
__SPFXD_END_DECLS
#endif
