/* lib-spfxd — index/rindex (legacy BSD names). */
#include <strings.h>
#include <string.h>

char *index(const char *s, int c) { return strchr(s, c); }
char *rindex(const char *s, int c) { return strrchr(s, c); }
