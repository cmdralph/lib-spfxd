/* lib-spfxd — getsubopt. */
#include <stdlib.h>
#include <string.h>

int getsubopt(char **opt, char *const *keys, char **val)
{
	char *s = *opt;
	*val = 0;
	*opt = strchrnul(s, ',');
	if (**opt) *(*opt)++ = 0;
	for (int i = 0; keys[i]; i++) {
		size_t l = strlen(keys[i]);
		if (strncmp(keys[i], s, l)) continue;
		if (s[l] == '=') *val = s + l + 1;
		else if (s[l]) continue;
		return i;
	}
	*val = s;
	return -1;
}
