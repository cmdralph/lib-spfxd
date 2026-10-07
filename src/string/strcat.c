/* lib-spfxd — strcat. */
#include <string.h>

char *strcat(char *restrict d, const char *restrict s)
{
	strcpy(d + strlen(d), s);
	return d;
}
