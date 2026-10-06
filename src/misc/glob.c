/*
 * lib-spfxd — glob / globfree.
 *
 * The pattern is split at '/'; components without metacharacters are
 * appended literally and only components with metacharacters cause a
 * directory scan, matched with fnmatch(FNM_PATHNAME|FNM_PERIOD).  Results
 * are sorted with strcmp unless GLOB_NOSORT.  Supports GLOB_ERR, GLOB_MARK,
 * GLOB_NOCHECK, GLOB_NOESCAPE, GLOB_APPEND, GLOB_DOOFFS, GLOB_PERIOD,
 * GLOB_ONLYDIR and GLOB_TILDE.
 */
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <glob.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct ctx {
	char **v;
	size_t n, cap;
	int flags;
	int (*errfunc)(const char *, int);
	int err;
};

static int has_meta(const char *s, size_t n, int noesc)
{
	for (size_t i = 0; i < n; i++) {
		if (s[i] == '*' || s[i] == '?' || s[i] == '[') return 1;
		if (s[i] == '\\' && !noesc && i + 1 < n) i++;
	}
	return 0;
}

static int add(struct ctx *c, const char *path, int mark)
{
	if (c->n + 1 >= c->cap) {
		size_t cap = c->cap ? 2 * c->cap : 16;
		char **nv = realloc(c->v, cap * sizeof *nv);
		if (!nv) return -1;
		c->v = nv;
		c->cap = cap;
	}
	size_t l = strlen(path);
	char *s = malloc(l + 2);
	if (!s) return -1;
	memcpy(s, path, l + 1);
	if (mark && (!l || s[l - 1] != '/')) {
		s[l] = '/';
		s[l + 1] = 0;
	}
	c->v[c->n++] = s;
	return 0;
}

/* Remove backslash escapes from a literal component. */
static void unescape(char *d, const char *s, size_t n, int noesc)
{
	for (size_t i = 0; i < n; i++) {
		if (s[i] == '\\' && !noesc && i + 1 < n) i++;
		*d++ = s[i];
	}
	*d = 0;
}

static int is_dir(const char *p)
{
	struct stat st;
	return !stat(p, &st) && S_ISDIR(st.st_mode);
}

static int expand(struct ctx *c, char *buf, size_t blen, const char *pat)
{
	int noesc = c->flags & GLOB_NOESCAPE;
	while (*pat == '/') {
		if (blen + 1 >= PATH_MAX) return 0;
		buf[blen++] = '/';
		buf[blen] = 0;
		pat++;
	}
	if (!*pat) {
		struct stat st;
		if (blen && lstat(buf, &st)) return 0;
		int dir = blen ? is_dir(buf) : 0;
		if ((c->flags & GLOB_ONLYDIR) && !dir) return 0;
		return add(c, buf, (c->flags & GLOB_MARK) && dir);
	}
	const char *end = strchrnul(pat, '/');
	size_t clen = (size_t)(end - pat);

	if (!has_meta(pat, clen, noesc)) {
		if (blen + clen + 1 >= PATH_MAX) return 0;
		unescape(buf + blen, pat, clen, noesc);
		size_t nl = strlen(buf);
		if (*end) {
			struct stat st;
			if (stat(buf, &st) || !S_ISDIR(st.st_mode)) return 0;
		}
		int r = expand(c, buf, nl, end);
		buf[blen] = 0;
		return r;
	}

	char comp[NAME_MAX + 1];
	if (clen > NAME_MAX) return 0;
	memcpy(comp, pat, clen);
	comp[clen] = 0;
	DIR *d = opendir(blen ? buf : ".");
	if (!d) {
		if ((c->errfunc && c->errfunc(blen ? buf : ".", errno)) || (c->flags & GLOB_ERR)) {
			c->err = GLOB_ABORTED;
			return -1;
		}
		return 0;
	}
	struct dirent *de;
	int r = 0;
	int fl = FNM_PERIOD | (noesc ? FNM_NOESCAPE : 0);
	if (c->flags & GLOB_PERIOD) fl &= ~FNM_PERIOD;
	while ((de = readdir(d))) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
			if (comp[0] != '.') continue;
		}
		if (fnmatch(comp, de->d_name, fl)) continue;
		size_t nl = strlen(de->d_name);
		if (blen + nl + 1 >= PATH_MAX) continue;
		memcpy(buf + blen, de->d_name, nl + 1);
		if (*end && de->d_type != DT_DIR && de->d_type != DT_UNKNOWN && de->d_type != DT_LNK) {
			buf[blen] = 0;
			continue;
		}
		r = expand(c, buf, blen + nl, end);
		buf[blen] = 0;
		if (r) break;
	}
	closedir(d);
	return r;
}

static int cmp(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

int glob(const char *restrict pat, int flags, int (*errfunc)(const char *, int), glob_t *restrict g)
{
	struct ctx c = { 0 };
	char buf[PATH_MAX];
	char tilde[PATH_MAX];
	size_t offs = (flags & GLOB_DOOFFS) ? g->gl_offs : 0;

	c.flags = flags;
	c.errfunc = errfunc;
	if (!(flags & GLOB_APPEND)) {
		g->gl_pathc = 0;
		g->gl_pathv = 0;
	}
	buf[0] = 0;
	if ((flags & GLOB_TILDE) && pat[0] == '~') {
		const char *rest = strchrnul(pat, '/');
		const char *home = 0;
		if (rest == pat + 1) {
			home = getenv("HOME");
		} else {
			char user[LOGIN_NAME_MAX];
			size_t ul = (size_t)(rest - pat - 1);
			if (ul < sizeof user) {
				memcpy(user, pat + 1, ul);
				user[ul] = 0;
				struct passwd *pw = getpwnam(user);
				if (pw) home = pw->pw_dir;
			}
		}
		if (home && strlen(home) + strlen(rest) < sizeof tilde) {
			strcpy(tilde, home);
			strcat(tilde, rest);
			pat = tilde;
		}
	}
	int r = expand(&c, buf, 0, pat);
	if (r && !c.err) c.err = GLOB_NOSPACE;
	if (c.err) {
		for (size_t i = 0; i < c.n; i++) free(c.v[i]);
		free(c.v);
		return c.err;
	}
	if (!c.n) {
		if (!(flags & GLOB_NOCHECK)) {
			free(c.v);
			return GLOB_NOMATCH;
		}
		if (add(&c, pat, 0)) return GLOB_NOSPACE;
	}
	if (!(flags & GLOB_NOSORT)) qsort(c.v, c.n, sizeof *c.v, cmp);

	size_t old = (flags & GLOB_APPEND) ? g->gl_pathc : 0;
	char **nv = realloc((flags & GLOB_APPEND) ? g->gl_pathv : 0,
		(offs + old + c.n + 1) * sizeof *nv);
	if (!nv) {
		for (size_t i = 0; i < c.n; i++) free(c.v[i]);
		free(c.v);
		return GLOB_NOSPACE;
	}
	if (!(flags & GLOB_APPEND))
		for (size_t i = 0; i < offs; i++) nv[i] = 0;
	memcpy(nv + offs + old, c.v, c.n * sizeof *nv);
	nv[offs + old + c.n] = 0;
	g->gl_pathv = nv;
	g->gl_pathc = old + c.n;
	g->gl_offs = offs;
	free(c.v);
	return 0;
}

void globfree(glob_t *g)
{
	if (!g->gl_pathv) return;
	for (size_t i = 0; i < g->gl_pathc; i++) free(g->gl_pathv[g->gl_offs + i]);
	free(g->gl_pathv);
	g->gl_pathc = 0;
	g->gl_pathv = 0;
}
