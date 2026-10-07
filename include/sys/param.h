/* lib-spfxd — <sys/param.h> */
#ifndef _SYS_PARAM_H
#define _SYS_PARAM_H
#include <limits.h>
#include <endian.h>
#define MAXSYMLINKS 20
#define MAXHOSTNAMELEN 64
#define MAXNAMLEN 255
#define MAXPATHLEN 4096
#define NBBY 8
#define NGROUPS 32
#define NOFILE 256
#define CANBSIZE 255
#define HZ 100
#define DEV_BSIZE 512
#define EXEC_PAGESIZE 4096
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#define howmany(n, d) (((n) + ((d) - 1)) / (d))
#define roundup(n, d) (howmany(n, d) * (d))
#define powerof2(n) !(((n) - 1) & (n))
#endif
