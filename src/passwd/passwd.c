/*
 * lib-spfxd — user and group databases from /etc/passwd and /etc/group.
 *
 * The _r functions parse into caller storage; the plain functions use a
 * per-database static record (growing as needed).  Lines are parsed in
 * place: fields split at ':', numeric ids validated.
 */
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "libc.h"

/* ---- passwd ---- */

static int parse_pw(char *line, struct passwd *pw)
{
	char *f[7];
	char *s = line;
	size_t l = strlen(s);
	if (l && s[l - 1] == '\n') s[l - 1] = 0;
	for (int i = 0; i < 7; i++) {
		f[i] = s;
		s = strchr(s, ':');
		if (!s && i < 6) return -1;
		if (s) *s++ = 0;
	}
	char *e;
	unsigned long uid = strtoul(f[2], &e, 10);
	if (*e || e == f[2] || uid > UINT_MAX) return -1;
	unsigned long gid = strtoul(f[3], &e, 10);
	if (*e || e == f[3] || gid > UINT_MAX) return -1;
	pw->pw_name = f[0];
	pw->pw_passwd = f[1];
	pw->pw_uid = (uid_t)uid;
	pw->pw_gid = (gid_t)gid;
	pw->pw_gecos = f[4];
	pw->pw_dir = f[5];
	pw->pw_shell = f[6];
	return 0;
}

/* Find a matching entry; copy the raw line into buf and parse there. */
static int pw_search(const char *name, uid_t uid, struct passwd *pw, char *buf, size_t size,
	struct passwd **res)
{
	char *line = 0;
	size_t cap = 0;
	int r = 0;
	*res = 0;
	FILE *f = fopen("/etc/passwd", "re");
	if (!f) return errno == ENOENT ? 0 : errno;
	while (getline(&line, &cap, f) >= 0) {
		struct passwd tmp;
		size_t l = strlen(line);
		char *dup = strdup(line);
		if (!dup) {
			r = ENOMEM;
			break;
		}
		if (parse_pw(dup, &tmp) || (name ? strcmp(tmp.pw_name, name) : tmp.pw_uid != uid)) {
			free(dup);
			continue;
		}
		free(dup);
		if (l + 1 > size) {
			r = ERANGE;
			break;
		}
		memcpy(buf, line, l + 1);
		parse_pw(buf, pw);
		*res = pw;
		break;
	}
	free(line);
	fclose(f);
	return r;
}

int getpwnam_r(const char *name, struct passwd *pw, char *buf, size_t size, struct passwd **res)
{
	return pw_search(name, 0, pw, buf, size, res);
}

int getpwuid_r(uid_t uid, struct passwd *pw, char *buf, size_t size, struct passwd **res)
{
	return pw_search(0, uid, pw, buf, size, res);
}

static struct passwd pw_static;
static char *pw_buf;
static size_t pw_size;

static struct passwd *pw_lookup(const char *name, uid_t uid)
{
	struct passwd *res;
	if (!pw_buf) {
		pw_size = 512;
		pw_buf = malloc(pw_size);
		if (!pw_buf) return 0;
	}
	for (;;) {
		int r = pw_search(name, uid, &pw_static, pw_buf, pw_size, &res);
		if (r != ERANGE) {
			if (r) errno = r;
			return res;
		}
		char *n = realloc(pw_buf, pw_size * 2);
		if (!n) return 0;
		pw_buf = n;
		pw_size *= 2;
	}
}

struct passwd *getpwnam(const char *name) { return pw_lookup(name, 0); }
struct passwd *getpwuid(uid_t uid) { return pw_lookup(0, uid); }

static FILE *pw_iter;
static char *iter_line;
static size_t iter_cap;

void setpwent(void) { if (pw_iter) rewind(pw_iter); }
void endpwent(void) { if (pw_iter) fclose(pw_iter); pw_iter = 0; }

struct passwd *fgetpwent(FILE *f)
{
	while (getline(&iter_line, &iter_cap, f) >= 0)
		if (!parse_pw(iter_line, &pw_static)) return &pw_static;
	return 0;
}

struct passwd *getpwent(void)
{
	if (!pw_iter && !(pw_iter = fopen("/etc/passwd", "re"))) return 0;
	return fgetpwent(pw_iter);
}

int putpwent(const struct passwd *pw, FILE *f)
{
	return fprintf(f, "%s:%s:%u:%u:%s:%s:%s\n", pw->pw_name, pw->pw_passwd, pw->pw_uid,
		pw->pw_gid, pw->pw_gecos, pw->pw_dir, pw->pw_shell) < 0 ? -1 : 0;
}

/* ---- group ---- */

/* Parse a group line into buf: members are stored as a pointer array at
 * the (aligned) start of buf followed by the line text. */
static int parse_gr(const char *line, struct group *gr, char *buf, size_t size)
{
	size_t l = strlen(line);
	size_t nmem = 1;
	for (const char *p = line; *p; p++) if (*p == ',') nmem++;
	size_t ptrs = (nmem + 1) * sizeof(char *);
	size_t align = (uintptr_t)buf % sizeof(char *) ? sizeof(char *) - (uintptr_t)buf % sizeof(char *) : 0;
	if (align + ptrs + l + 1 > size) return ERANGE;
	char **mem = (char **)(void *)(buf + align);
	char *s = buf + align + ptrs;
	memcpy(s, line, l + 1);
	if (l && s[l - 1] == '\n') s[l - 1] = 0;
	char *f[4];
	for (int i = 0; i < 4; i++) {
		f[i] = s;
		s = strchr(s, ':');
		if (!s && i < 3) return EINVAL;
		if (s) *s++ = 0;
	}
	char *e;
	unsigned long gid = strtoul(f[2], &e, 10);
	if (*e || e == f[2] || gid > UINT_MAX) return EINVAL;
	size_t k = 0;
	if (*f[3]) {
		for (char *p = f[3];;) {
			mem[k++] = p;
			p = strchr(p, ',');
			if (!p) break;
			*p++ = 0;
		}
	}
	mem[k] = 0;
	gr->gr_name = f[0];
	gr->gr_passwd = f[1];
	gr->gr_gid = (gid_t)gid;
	gr->gr_mem = mem;
	return 0;
}

static int gr_search(const char *name, gid_t gid, struct group *gr, char *buf, size_t size,
	struct group **res)
{
	char *line = 0;
	size_t cap = 0;
	int r = 0;
	*res = 0;
	FILE *f = fopen("/etc/group", "re");
	if (!f) return errno == ENOENT ? 0 : errno;
	while (getline(&line, &cap, f) >= 0) {
		/* quick pre-check on the name / id fields */
		char *c1 = strchr(line, ':');
		if (!c1) continue;
		if (name) {
			if ((size_t)(c1 - line) != strlen(name) || strncmp(line, name, (size_t)(c1 - line))) continue;
		} else {
			char *c2 = strchr(c1 + 1, ':');
			if (!c2 || strtoul(c2 + 1, 0, 10) != gid) continue;
		}
		int e = parse_gr(line, gr, buf, size);
		if (e == EINVAL) continue;
		if (e) r = e;
		else *res = gr;
		break;
	}
	free(line);
	fclose(f);
	return r;
}

int getgrnam_r(const char *name, struct group *gr, char *buf, size_t size, struct group **res)
{
	return gr_search(name, 0, gr, buf, size, res);
}

int getgrgid_r(gid_t gid, struct group *gr, char *buf, size_t size, struct group **res)
{
	return gr_search(0, gid, gr, buf, size, res);
}

static struct group gr_static;
static char *gr_buf;
static size_t gr_size;

static struct group *gr_lookup(const char *name, gid_t gid)
{
	struct group *res;
	if (!gr_buf) {
		gr_size = 1024;
		gr_buf = malloc(gr_size);
		if (!gr_buf) return 0;
	}
	for (;;) {
		int r = gr_search(name, gid, &gr_static, gr_buf, gr_size, &res);
		if (r != ERANGE) {
			if (r) errno = r;
			return res;
		}
		char *n = realloc(gr_buf, gr_size * 2);
		if (!n) return 0;
		gr_buf = n;
		gr_size *= 2;
	}
}

struct group *getgrnam(const char *name) { return gr_lookup(name, 0); }
struct group *getgrgid(gid_t gid) { return gr_lookup(0, gid); }

static FILE *gr_iter;

void setgrent(void) { if (gr_iter) rewind(gr_iter); }
void endgrent(void) { if (gr_iter) fclose(gr_iter); gr_iter = 0; }

struct group *getgrent(void)
{
	if (!gr_iter && !(gr_iter = fopen("/etc/group", "re"))) return 0;
	if (!gr_buf) {
		gr_size = 1024;
		if (!(gr_buf = malloc(gr_size))) return 0;
	}
	while (getline(&iter_line, &iter_cap, gr_iter) >= 0) {
		int e;
		while ((e = parse_gr(iter_line, &gr_static, gr_buf, gr_size)) == ERANGE) {
			char *n = realloc(gr_buf, gr_size * 2);
			if (!n) return 0;
			gr_buf = n;
			gr_size *= 2;
		}
		if (!e) return &gr_static;
	}
	return 0;
}

int getgrouplist(const char *user, gid_t gid, gid_t *groups, int *ngroups)
{
	int n = 0, max = *ngroups;
	struct group *g;
	if (n < max) groups[n] = gid;
	n++;
	endgrent();
	while ((g = getgrent())) {
		if (g->gr_gid == gid) continue;
		for (char **m = g->gr_mem; *m; m++) {
			if (!strcmp(*m, user)) {
				if (n < max) groups[n] = g->gr_gid;
				n++;
				break;
			}
		}
	}
	endgrent();
	*ngroups = n;
	return n > max ? -1 : n;
}

int initgroups(const char *user, gid_t gid)
{
	gid_t groups[NGROUPS_MAX < 1024 ? NGROUPS_MAX : 1024];
	int n = (int)(sizeof groups / sizeof *groups);
	if (getgrouplist(user, gid, groups, &n) < 0) n = (int)(sizeof groups / sizeof *groups);
	return setgroups((size_t)n, groups);
}
