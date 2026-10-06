/* lib-spfxd — <dlfcn.h> */
#ifndef _DLFCN_H
#define _DLFCN_H
#include <features.h>

#define RTLD_LAZY     1
#define RTLD_NOW      2
#define RTLD_NOLOAD   4
#define RTLD_NODELETE 4096
#define RTLD_GLOBAL   256
#define RTLD_LOCAL    0
#define RTLD_NEXT    ((void *)-1)
#define RTLD_DEFAULT ((void *)0)

__SPFXD_BEGIN_DECLS
int dlclose(void *);
char *dlerror(void);
void *dlopen(const char *, int);
void *dlsym(void *__restrict, const char *__restrict);
#if defined(__SPFXD_GNU) || defined(__SPFXD_BSD)
typedef struct {
	const char *dli_fname;
	void *dli_fbase;
	const char *dli_sname;
	void *dli_saddr;
} Dl_info;
int dladdr(const void *, Dl_info *);
#endif
__SPFXD_END_DECLS
#endif
