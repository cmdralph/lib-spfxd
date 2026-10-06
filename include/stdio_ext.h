/* lib-spfxd — <stdio_ext.h> (Solaris/glibc stream inspection interface) */
#ifndef _STDIO_EXT_H
#define _STDIO_EXT_H
#include <stdio.h>

__SPFXD_BEGIN_DECLS

#define FSETLOCKING_QUERY 0
#define FSETLOCKING_INTERNAL 1
#define FSETLOCKING_BYCALLER 2

size_t __fbufsize(FILE *);
size_t __fpending(FILE *);
int __flbf(FILE *);
int __freadable(FILE *);
int __fwritable(FILE *);
int __freading(FILE *);
int __fwriting(FILE *);
int __fsetlocking(FILE *, int);
void _flushlbf(void);
void __fpurge(FILE *);
void __fseterr(FILE *);
size_t __freadahead(FILE *);
const char *__freadptr(FILE *, size_t *);
void __freadptrinc(FILE *, size_t);

__SPFXD_END_DECLS
#endif
