/*
 * lib-spfxd — realpath.
 *
 * Resolves the path textually, component by component, following symbolic
 * links with readlink and verifying each prefix exists.  This works without
 * /proc and handles "..", ".", repeated slashes and relative links.
 */
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char *realpath(const char *restrict path, char *restrict resolved)
{
	char out[PATH_MAX], rest[PATH_MAX], link[PATH_MAX];
	size_t olen = 0, restlen;
	int links = 0;

	if (!path) {
		errno = EINVAL;
		return 0;
	}
	if (!*path) {
		errno = ENOENT;
		return 0;
	}
	restlen = strnlen(path, sizeof rest);
	if (restlen == sizeof rest) {
		errno = ENAMETOOLONG;
		return 0;
	}
	memcpy(rest, path, restlen + 1);

	if (rest[0] != '/') {
		if (!getcwd(out, sizeof out)) return 0;
		olen = strlen(out);
		if (olen == 1) olen = 0;   /* cwd is "/" */
	}

	char *p = rest;
	while (*p) {
		while (*p == '/') p++;
		if (!*p) break;
		char *e = strchrnul(p, '/');
		size_t clen = (size_t)(e - p);

		if (clen == 1 && p[0] == '.') {
			p = e;
			continue;
		}
		if (clen == 2 && p[0] == '.' && p[1] == '.') {
			while (olen && out[olen - 1] != '/') olen--;
			if (olen) olen--;
			p = e;
			continue;
		}
		if (olen + 1 + clen >= sizeof out) {
			errno = ENAMETOOLONG;
			return 0;
		}
		out[olen] = '/';
		memcpy(out + olen + 1, p, clen);
		size_t newlen = olen + 1 + clen;
		out[newlen] = 0;

		ssize_t k = readlink(out, link, sizeof link);
		if (k < 0) {
			if (errno != EINVAL) return 0;   /* not a link: must exist */
			olen = newlen;
			p = e;
			continue;
		}
		if ((size_t)k == sizeof link || ++links > SYMLOOP_MAX) {
			errno = (size_t)k == sizeof link ? ENAMETOOLONG : ELOOP;
			return 0;
		}
		/* Splice the link target in front of the unresolved remainder. */
		size_t remlen = strlen(e);
		if ((size_t)k + remlen + 1 > sizeof rest) {
			errno = ENAMETOOLONG;
			return 0;
		}
		memmove(rest + k, e, remlen + 1);
		memcpy(rest, link, (size_t)k);
		p = rest;
		if (link[0] == '/') olen = 0;
	}
	if (!olen) out[olen++] = '/';
	out[olen] = 0;

	/* The final path must exist (readlink above already checked every
	 * component except when the path was only "/" or ended in "..") */
	struct stat st;
	if (stat(out, &st) < 0) return 0;
	if (path[strlen(path) - 1] == '/' && !S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		return 0;
	}
	return resolved ? strcpy(resolved, out) : strdup(out);
}

char *canonicalize_file_name(const char *path)
{
	return realpath(path, 0);
}
