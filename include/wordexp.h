/* lib-spfxd — <wordexp.h> */
#ifndef _WORDEXP_H
#define _WORDEXP_H
#include <features.h>
#define __SPFXD_NEED_size_t
#include <bits/typedefs.h>

__SPFXD_BEGIN_DECLS

#define WRDE_DOOFFS  1
#define WRDE_APPEND  2
#define WRDE_NOCMD   4
#define WRDE_REUSE   8
#define WRDE_SHOWERR 16
#define WRDE_UNDEF   32

#define WRDE_NOSPACE 1
#define WRDE_BADCHAR 2
#define WRDE_BADVAL  3
#define WRDE_CMDSUB  4
#define WRDE_SYNTAX  5
#define WRDE_NOSYS   -1

typedef struct {
	size_t we_wordc;
	char **we_wordv;
	size_t we_offs;
} wordexp_t;

int wordexp(const char *__restrict, wordexp_t *__restrict, int);
void wordfree(wordexp_t *);

__SPFXD_END_DECLS
#endif
