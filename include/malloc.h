/* lib-spfxd — <malloc.h> */
#ifndef _MALLOC_H
#define _MALLOC_H
#include <features.h>
#define __SPFXD_NEED_size_t
#include <bits/typedefs.h>
__SPFXD_BEGIN_DECLS
void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);
void *valloc(size_t);
void *pvalloc(size_t);
void *memalign(size_t, size_t);
size_t malloc_usable_size(void *);
int malloc_trim(size_t);
__SPFXD_END_DECLS
#endif
