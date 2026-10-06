/* lib-spfxd — strtok / strtok_r / strsep. */
#include <string.h>

char *strtok_r(char *restrict s, const char *restrict sep, char **restrict save)
{
	if (!s && !(s = *save)) return 0;
	s += strspn(s, sep);
	if (!*s) return *save = 0;
	*save = s + strcspn(s, sep);
	if (**save) *(*save)++ = 0;
	else *save = 0;
	return s;
}

char *strtok(char *restrict s, const char *restrict sep)
{
	static char *save;
	return strtok_r(s, sep, &save);
}

char *strsep(char **str, const char *sep)
{
	char *s = *str, *end;
	if (!s) return 0;
	end = s + strcspn(s, sep);
	if (*end) *end++ = 0;
	else end = 0;
	*str = end;
	return s;
}
