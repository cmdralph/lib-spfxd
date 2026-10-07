/*
 * lib-spfxd — message catalogs (<nl_types.h>).
 *
 * Message catalogs are not supported: catopen always fails with ENOENT
 * (as it does on any system where the requested catalog is not installed),
 * and catgets returns the caller's default message, which is the
 * behaviour POSIX specifies for an unavailable catalog.
 */
#include <errno.h>
#include <nl_types.h>

nl_catd catopen(const char *name, int flag)
{
	errno = ENOENT;
	return (nl_catd)-1;
}

char *catgets(nl_catd cat, int set, int id, const char *s)
{
	errno = EBADF;
	return (char *)s;
}

int catclose(nl_catd cat)
{
	errno = EBADF;
	return -1;
}
