/* lib-spfxd — <search.h> */
#ifndef _SEARCH_H
#define _SEARCH_H
#include <features.h>
#define __SPFXD_NEED_size_t
#include <bits/typedefs.h>
typedef enum { FIND, ENTER } ACTION;
typedef enum { preorder, postorder, endorder, leaf } VISIT;
typedef struct entry { char *key; void *data; } ENTRY;
__SPFXD_BEGIN_DECLS
int hcreate(size_t);
void hdestroy(void);
ENTRY *hsearch(ENTRY, ACTION);
void insque(void *, void *);
void remque(void *);
void *lsearch(const void *, void *, size_t *, size_t, int (*)(const void *, const void *));
void *lfind(const void *, const void *, size_t *, size_t, int (*)(const void *, const void *));
void *tdelete(const void *__restrict, void **__restrict, int (*)(const void *, const void *));
void *tfind(const void *, void *const *, int (*)(const void *, const void *));
void *tsearch(const void *, void **, int (*)(const void *, const void *));
void twalk(const void *, void (*)(const void *, VISIT, int));
#if defined(__SPFXD_GNU)
void tdestroy(void *, void (*)(void *));
#endif
__SPFXD_END_DECLS
#endif
