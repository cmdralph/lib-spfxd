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
#ifdef __SPFXD_GNU
/* glibc 2.35 interface used by unwinders: locate the object containing an
 * address and its PT_GNU_EH_FRAME data */
struct link_map;
struct dl_find_object {
	unsigned long long dlfo_flags;
	void *dlfo_map_start;
	void *dlfo_map_end;
	struct link_map *dlfo_link_map;
	void *dlfo_eh_frame;
	unsigned long long __dlfo_reserved[7];
};
#define DLFO_STRUCT_HAS_EH_DBASE 0
#define DLFO_STRUCT_HAS_EH_COUNT 0
#define DLFO_EH_SEGMENT_TYPE 0x6474e550
int _dl_find_object(void *, struct dl_find_object *);
#endif
__SPFXD_END_DECLS
#endif
