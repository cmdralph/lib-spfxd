/* lib-spfxd — strdup / strndup / basename (GNU) / dirname / __xpg_basename. */
#include <string.h>
#include <stdlib.h>
#include <libgen.h>

char *strdup(const char *s)
{
	size_t l = strlen(s) + 1;
	char *d = malloc(l);
	return d ? memcpy(d, s, l) : 0;
}

char *strndup(const char *s, size_t n)
{
	size_t l = strnlen(s, n);
	char *d = malloc(l + 1);
	if (!d) return 0;
	memcpy(d, s, l);
	d[l] = 0;
	return d;
}

#undef basename
/* GNU basename: never modifies its argument; "a/b/" yields "". */
char *basename(const char *path)
{
	const char *p = strrchr(path, '/');
	return (char *)(p ? p + 1 : path);
}

/* POSIX basename: strips trailing slashes (modifying the string). */
char *__xpg_basename(char *s)
{
	if (!s || !*s) return (char *)".";
	size_t i = strlen(s) - 1;
	for (; i && s[i] == '/'; i--) s[i] = 0;
	for (; i && s[i - 1] != '/'; i--);
	return s + i;
}

char *dirname(char *s)
{
	if (!s || !*s) return (char *)".";
	size_t i = strlen(s) - 1;
	for (; s[i] == '/'; i--) if (!i) return (char *)"/";
	for (; s[i] != '/'; i--) if (!i) return (char *)".";
	for (; s[i] == '/'; i--) if (!i) return (char *)"/";
	s[i + 1] = 0;
	return s;
}
