/*
 * lib-spfxd — the environment: getenv, setenv, unsetenv, putenv, clearenv,
 * secure_getenv.
 *
 * Strings created by setenv are owned by the library; when such a string
 * is replaced or removed it is freed (tracked in a small owned-string
 * list), while strings passed to putenv are never freed (POSIX: they
 * become part of the environment).  Modifications are serialized; getenv
 * only reads.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "libc.h"
#include "lock.h"

static volatile int env_lock;
static char **owned;          /* strings allocated by setenv */
static size_t owned_n, owned_cap;
static char **env_alloced;    /* environ array allocated by us, if any */

char *getenv(const char *name)
{
	size_t l = strchrnul(name, '=') - name;
	if (l && !name[l] && __environ) {
		/* compare the first two bytes inline (the second is '=' or the
		 * terminator for one-character names, which still must match)
		 * before calling strncmp: almost every entry is rejected there */
		unsigned char c0 = (unsigned char)name[0], c1 = (unsigned char)name[1];
		if (!c1) c1 = '=';
		for (char **e = __environ; *e; e++) {
			const unsigned char *v = (const unsigned char *)*e;
			if (v[0] != c0 || v[1] != c1) continue;
			if (!strncmp(name, *e, l) && (*e)[l] == '=') return *e + l + 1;
		}
	}
	return 0;
}

char *secure_getenv(const char *name)
{
	return __libc.secure ? 0 : getenv(name);
}

static void forget_owned(char *s)
{
	for (size_t i = 0; i < owned_n; i++) {
		if (owned[i] == s) {
			owned[i] = owned[--owned_n];
			free(s);
			return;
		}
	}
}

static int remember_owned(char *s)
{
	if (owned_n == owned_cap) {
		size_t cap = owned_cap ? 2 * owned_cap : 16;
		char **n = realloc(owned, cap * sizeof *n);
		if (!n) return -1;
		owned = n;
		owned_cap = cap;
	}
	owned[owned_n++] = s;
	return 0;
}

/* Install "name=value" string s (replacing any entry with the same name).
 * Called with env_lock held. */
static int env_install(char *s, size_t namelen)
{
	size_t i = 0;
	if (__environ) {
		for (; __environ[i]; i++) {
			if (!strncmp(s, __environ[i], namelen) && __environ[i][namelen] == '=') {
				char *old = __environ[i];
				__environ[i] = s;
				forget_owned(old);
				return 0;
			}
		}
	}
	char **n;
	if (__environ == env_alloced) {
		n = realloc(env_alloced, (i + 2) * sizeof *n);
		if (!n) return -1;
	} else {
		n = malloc((i + 2) * sizeof *n);
		if (!n) return -1;
		if (i) memcpy(n, __environ, i * sizeof *n);
	}
	n[i] = s;
	n[i + 1] = 0;
	__environ = env_alloced = n;
	return 0;
}

int setenv(const char *name, const char *value, int overwrite)
{
	size_t l = strchrnul(name, '=') - name;
	if (!l || name[l]) {
		errno = EINVAL;
		return -1;
	}
	if (!overwrite && getenv(name)) return 0;
	size_t vl = strlen(value);
	char *s = malloc(l + vl + 2);
	if (!s) return -1;
	memcpy(s, name, l);
	s[l] = '=';
	memcpy(s + l + 1, value, vl + 1);

	__lock(&env_lock);
	int r = -1;
	if (!remember_owned(s)) {
		r = env_install(s, l);
		if (r) owned_n--;
	}
	__unlock(&env_lock);
	if (r) free(s);
	return r;
}

int unsetenv(const char *name)
{
	size_t l = strchrnul(name, '=') - name;
	if (!l || name[l]) {
		errno = EINVAL;
		return -1;
	}
	__lock(&env_lock);
	if (__environ) {
		char **e = __environ, **o = e;
		for (; *e; e++) {
			if (!strncmp(name, *e, l) && (*e)[l] == '=') {
				forget_owned(*e);
				continue;
			}
			*o++ = *e;
		}
		*o = 0;
	}
	__unlock(&env_lock);
	return 0;
}

int putenv(char *s)
{
	size_t l = strchrnul(s, '=') - s;
	if (!l) {
		errno = EINVAL;
		return -1;
	}
	if (!s[l]) return unsetenv(s);
	__lock(&env_lock);
	int r = env_install(s, l);
	__unlock(&env_lock);
	return r;
}

int clearenv(void)
{
	__lock(&env_lock);
	while (owned_n) free(owned[--owned_n]);
	free(env_alloced);
	env_alloced = 0;
	__environ = 0;
	__unlock(&env_lock);
	return 0;
}
