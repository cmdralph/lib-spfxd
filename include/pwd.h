/* lib-spfxd — <pwd.h> (backed by /etc/passwd) */
#ifndef _PWD_H
#define _PWD_H
#include <features.h>
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_uid_t
#define __SPFXD_NEED_gid_t
#define __SPFXD_NEED_FILE
#include <bits/typedefs.h>
struct passwd {
	char *pw_name;
	char *pw_passwd;
	uid_t pw_uid;
	gid_t pw_gid;
	char *pw_gecos;
	char *pw_dir;
	char *pw_shell;
};
__SPFXD_BEGIN_DECLS
struct passwd *getpwuid(uid_t);
struct passwd *getpwnam(const char *);
int getpwuid_r(uid_t, struct passwd *, char *, size_t, struct passwd **);
int getpwnam_r(const char *, struct passwd *, char *, size_t, struct passwd **);
#if defined(__SPFXD_XSI)
void setpwent(void);
void endpwent(void);
struct passwd *getpwent(void);
#endif
#if defined(__SPFXD_GNU)
struct passwd *fgetpwent(FILE *);
int putpwent(const struct passwd *, FILE *);
#endif
__SPFXD_END_DECLS
#endif
