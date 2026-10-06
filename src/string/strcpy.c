/* lib-spfxd — strcpy. */
#include <string.h>

char *strcpy(char *restrict d, const char *restrict s)
{
	stpcpy(d, s);
	return d;
}
