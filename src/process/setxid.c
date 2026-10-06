/*
 * lib-spfxd — the set*id family.  Linux keeps credentials per thread, so
 * after the calling thread succeeds the same change is applied on every
 * other thread through __synccall, giving POSIX process-wide semantics.
 */
#include <errno.h>
#include <unistd.h>
#include <grp.h>
#include "pthread_impl.h"

struct setxid_ctx { long nr, a, b, c; };

static void apply(void *p)
{
	struct setxid_ctx *c = p;
	__syscall(c->nr, c->a, c->b, c->c);
}

static int setxid(long nr, long a, long b, long c)
{
	long r = __syscall(nr, a, b, c);
	if (r < 0) return (int)__syscall_ret((unsigned long)r);
	struct setxid_ctx ctx = { nr, a, b, c };
	__synccall(apply, &ctx);
	return 0;
}

int setuid(uid_t uid) { return setxid(SYS_setuid, uid, 0, 0); }
int setgid(gid_t gid) { return setxid(SYS_setgid, gid, 0, 0); }
int seteuid(uid_t e) { return setxid(SYS_setresuid, -1, e, -1); }
int setegid(gid_t e) { return setxid(SYS_setresgid, -1, e, -1); }
int setreuid(uid_t r, uid_t e) { return setxid(SYS_setreuid, r, e, 0); }
int setregid(gid_t r, gid_t e) { return setxid(SYS_setregid, r, e, 0); }
int setresuid(uid_t r, uid_t e, uid_t s) { return setxid(SYS_setresuid, r, e, s); }
int setresgid(gid_t r, gid_t e, gid_t s) { return setxid(SYS_setresgid, r, e, s); }

int setgroups(size_t n, const gid_t *list)
{
	return setxid(SYS_setgroups, (long)n, (long)list, 0);
}
