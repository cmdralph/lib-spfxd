/* lib-spfxd — <dirent.h> */
#ifndef _DIRENT_H
#define _DIRENT_H
#include <features.h>

#define __SPFXD_NEED_ino_t
#define __SPFXD_NEED_off_t
#define __SPFXD_NEED_size_t
#define __SPFXD_NEED_ssize_t
#include <bits/typedefs.h>

typedef struct __spfxd_dir DIR;

/* Matches the kernel's struct linux_dirent64, so entries are returned
 * directly out of the getdents64 buffer without copying. */
struct dirent {
	ino_t d_ino;
	off_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[256];
};
#define d_fileno d_ino

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14
#define IFTODT(x) ((x) >> 12 & 017)
#define DTTOIF(x) ((x) << 12)

__SPFXD_BEGIN_DECLS
int closedir(DIR *);
DIR *fdopendir(int);
DIR *opendir(const char *);
struct dirent *readdir(DIR *);
int readdir_r(DIR *__restrict, struct dirent *__restrict, struct dirent **__restrict);
void rewinddir(DIR *);
int dirfd(DIR *);
int alphasort(const struct dirent **, const struct dirent **);
int scandir(const char *, struct dirent ***, int (*)(const struct dirent *),
	int (*)(const struct dirent **, const struct dirent **));
#if defined(__SPFXD_XSI)
void seekdir(DIR *, long);
long telldir(DIR *);
#endif
#if defined(__SPFXD_GNU)
int versionsort(const struct dirent **, const struct dirent **);
ssize_t getdents64(int, void *, size_t);
#endif
__SPFXD_END_DECLS
#endif
