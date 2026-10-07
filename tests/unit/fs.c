/* lib-spfxd test — files and directories through the POSIX API. */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <ftw.h>
#include <glob.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "t.h"

static char root[64];
static int nftw_files, nftw_dirs;
static int walker(const char *p, const struct stat *st, int type, struct FTW *f)
{
	(void)p; (void)st; (void)f;
	if (type == FTW_F) nftw_files++;
	else if (type == FTW_D || type == FTW_DP) nftw_dirs++;
	return 0;
}
static int rm(const char *p, const struct stat *st, int type, struct FTW *f)
{
	(void)st; (void)type; (void)f;
	return remove(p);
}

int main(void)
{
	char path[PATH_MAX], buf[256];
	strcpy(root, "/tmp/spfxd_fsXXXXXX");
	CHECK(mkdtemp(root) != NULL, "mkdtemp");
	CHECK(!chdir(root), "chdir");
	CHECK(getcwd(path, sizeof path) && !strcmp(path, root), "getcwd '%s'", path);
	char *cwd = getcwd(0, 0);
	CHECK(cwd && !strcmp(cwd, root), "getcwd(NULL)");
	free(cwd);

	/* open/read/write/seek */
	int fd = open("f", O_RDWR | O_CREAT | O_EXCL, 0640);
	CHECK(fd >= 0, "open O_CREAT|O_EXCL");
	CHECK(open("f", O_RDWR | O_CREAT | O_EXCL, 0640) == -1 && errno == EEXIST, "O_EXCL fails on existing");
	CHECK(write(fd, "hello world", 11) == 11, "write");
	CHECK(lseek(fd, 6, SEEK_SET) == 6 && read(fd, buf, 5) == 5 && !memcmp(buf, "world", 5), "lseek+read");
	CHECK(pwrite(fd, "W", 1, 6) == 1 && pread(fd, buf, 5, 6) == 5 && !memcmp(buf, "World", 5), "pread/pwrite");
	struct iovec iov[2] = { { "ab", 2 }, { "cd", 2 } };
	lseek(fd, 0, SEEK_END);
	CHECK(writev(fd, iov, 2) == 4, "writev");
	char r1[3] = { 0 }, r2[3] = { 0 };
	struct iovec riov[2] = { { r1, 2 }, { r2, 2 } };
	CHECK(preadv(fd, riov, 2, 11) == 4 && !strcmp(r1, "ab") && !strcmp(r2, "cd"), "preadv");
	CHECK(!ftruncate(fd, 5) && lseek(fd, 0, SEEK_END) == 5, "ftruncate");
	CHECK(!fsync(fd) && !fdatasync(fd), "fsync");
	/* stat */
	struct stat st;
	CHECK(!fstat(fd, &st) && st.st_size == 5 && S_ISREG(st.st_mode) && (st.st_mode & 0777) == (0640 & ~umask(022)), "fstat mode %o", st.st_mode & 0777);
	umask(022);
	CHECK(!fchmod(fd, 0600) && !stat("f", &st) && (st.st_mode & 0777) == 0600, "fchmod");
	CHECK(!fstatat(AT_FDCWD, "f", &st, 0) && st.st_nlink == 1, "fstatat");
	/* fcntl */
	CHECK(!(fcntl(fd, F_GETFD) & FD_CLOEXEC) && !fcntl(fd, F_SETFD, FD_CLOEXEC) && (fcntl(fd, F_GETFD) & FD_CLOEXEC), "FD_CLOEXEC");
	int fd2 = fcntl(fd, F_DUPFD_CLOEXEC, 100);
	CHECK(fd2 >= 100, "F_DUPFD_CLOEXEC");
	close(fd2);
	struct flock fl = { .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 1 };
	CHECK(!fcntl(fd, F_SETLK, &fl), "record lock");
	CHECK(dup2(fd, 50) == 50 && dup3(fd, 51, O_CLOEXEC) == 51, "dup2/dup3");
	close(50);
	close(51);
	/* mmap a file */
	char *m = mmap(0, 5, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	CHECK(m != MAP_FAILED && !memcmp(m, "hello", 5), "mmap file");
	m[0] = 'J';
	CHECK(!msync(m, 5, MS_SYNC) && pread(fd, buf, 1, 0) == 1 && buf[0] == 'J', "msync");
	munmap(m, 5);
	close(fd);
	/* links, rename, readlink, access */
	CHECK(!link("f", "hard") && !stat("f", &st) && st.st_nlink == 2, "link");
	CHECK(!symlink("f", "sym") && readlink("sym", buf, sizeof buf) == 1 && buf[0] == 'f', "symlink/readlink");
	CHECK(!lstat("sym", &st) && S_ISLNK(st.st_mode) && !stat("sym", &st) && S_ISREG(st.st_mode), "lstat vs stat");
	CHECK(!rename("hard", "hard2") && access("hard", F_OK) == -1 && !access("hard2", R_OK | W_OK), "rename/access");
	CHECK(!renameat(AT_FDCWD, "hard2", AT_FDCWD, "hard3"), "renameat");
	CHECK(!unlinkat(AT_FDCWD, "hard3", 0) && !unlink("sym"), "unlinkat");
	CHECK(faccessat(AT_FDCWD, "nope", F_OK, 0) == -1 && errno == ENOENT, "faccessat");
	/* times */
	struct timespec ts[2] = { { 1000000000, 0 }, { 1234567890, 500000000 } };
	CHECK(!utimensat(AT_FDCWD, "f", ts, 0) && !stat("f", &st) && st.st_mtim.tv_sec == 1234567890 &&
	      st.st_mtim.tv_nsec == 500000000 && st.st_atim.tv_sec == 1000000000, "utimensat");
	struct utimbuf ub = { 5, 6 };
	CHECK(!utime("f", &ub) && !stat("f", &st) && st.st_mtime == 6, "utime");
	/* directories */
	CHECK(!mkdir("d", 0755) && mkdir("d", 0755) == -1 && errno == EEXIST, "mkdir");
	for (int i = 0; i < 20; i++) {
		snprintf(path, sizeof path, "d/file%02d", i);
		close(open(path, O_CREAT | O_WRONLY, 0644));
	}
	mkdir("d/sub", 0755);
	close(open("d/sub/x.c", O_CREAT | O_WRONLY, 0644));
	DIR *dir = opendir("d");
	CHECK(dir != NULL, "opendir");
	int n = 0, saw_dot = 0;
	long pos = -1;
	struct dirent *e;
	while ((e = readdir(dir))) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) { saw_dot++; continue; }
		if (n == 5) pos = telldir(dir);
		n++;
	}
	CHECK(n == 21 && saw_dot == 2, "readdir count %d", n);
	rewinddir(dir);
	int n2 = 0;
	while (readdir(dir)) n2++;
	CHECK(n2 == 23, "rewinddir");
	seekdir(dir, pos);
	int n3 = 0;
	while (readdir(dir)) n3++;
	CHECK(n3 > 0 && n3 < 23, "seekdir/telldir (%d)", n3);
	CHECK(dirfd(dir) >= 0, "dirfd");
	closedir(dir);
	struct dirent **list;
	int ns = scandir("d", &list, 0, alphasort);
	CHECK(ns == 23 && !strcmp(list[0]->d_name, ".") && !strcmp(list[2]->d_name, "file00") && !strcmp(list[22]->d_name, "sub"),
	      "scandir+alphasort");
	for (int i = 0; i < ns; i++) free(list[i]);
	free(list);
	CHECK(rmdir("d") == -1 && (errno == ENOTEMPTY || errno == EEXIST), "rmdir non-empty");
	/* nftw */
	CHECK(!nftw("d", walker, 8, FTW_PHYS), "nftw");
	CHECK(nftw_files == 21 && nftw_dirs == 2, "nftw counts %d files %d dirs", nftw_files, nftw_dirs);
	/* glob and fnmatch */
	glob_t g;
	CHECK(!glob("d/file1*", 0, 0, &g) && g.gl_pathc == 10 && !strcmp(g.gl_pathv[0], "d/file10"), "glob");
	globfree(&g);
	CHECK(glob("d/*.none", 0, 0, &g) == GLOB_NOMATCH, "glob no match");
	CHECK(!glob("d/*.none", GLOB_NOCHECK, 0, &g) && g.gl_pathc == 1, "GLOB_NOCHECK");
	globfree(&g);
	CHECK(!glob("d/s?b/*.[ch]", 0, 0, &g) && g.gl_pathc == 1, "glob nested");
	globfree(&g);
	CHECK(!fnmatch("*.c", "x.c", 0) && fnmatch("*.c", "x.h", 0) == FNM_NOMATCH, "fnmatch");
	CHECK(fnmatch("*", ".hidden", FNM_PERIOD) == FNM_NOMATCH && !fnmatch("a/*", "a/b", FNM_PATHNAME) &&
	      fnmatch("*", "a/b", FNM_PATHNAME) == FNM_NOMATCH, "fnmatch flags");
	CHECK(!fnmatch("[!a-c]x", "dx", 0) && !fnmatch("\\*", "*", 0) && !fnmatch("[[:digit:]]", "5", 0), "fnmatch classes");
	CHECK(!fnmatch("ABC", "abc", FNM_CASEFOLD), "FNM_CASEFOLD");
	/* misc */
	CHECK(!mkfifo("fifo", 0600) && !stat("fifo", &st) && S_ISFIFO(st.st_mode), "mkfifo");
	struct statvfs sv;
	CHECK(!statvfs(".", &sv) && sv.f_bsize > 0, "statvfs");
	CHECK(pathconf(".", _PC_NAME_MAX) >= 14, "pathconf");
	CHECK(!truncate("f", 2) && !stat("f", &st) && st.st_size == 2, "truncate");
	CHECK(!chdir("/") && !nftw(root, rm, 8, FTW_DEPTH | FTW_PHYS) && access(root, F_OK) == -1, "recursive removal");
	return DONE();
}
