/* lib-spfxd — ffs/ffsl/ffsll: 1-based index of the lowest set bit. */
#include <strings.h>

int ffs(int i) { return i ? __builtin_ctz((unsigned)i) + 1 : 0; }
int ffsl(long i) { return i ? __builtin_ctzl((unsigned long)i) + 1 : 0; }
int ffsll(long long i) { return i ? __builtin_ctzll((unsigned long long)i) + 1 : 0; }
