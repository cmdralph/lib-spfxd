/* lib-spfxd — <sys/random.h> */
#ifndef _SYS_RANDOM_H
#define _SYS_RANDOM_H
#include <features.h>
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_ssize_t
#include <bits/typedefs.h>
#define GRND_NONBLOCK 1
#define GRND_RANDOM   2
#define GRND_INSECURE 4
__SPFXD_BEGIN_DECLS
ssize_t getrandom(void *, size_t, unsigned);
int getentropy(void *, size_t);
__SPFXD_END_DECLS
#endif
