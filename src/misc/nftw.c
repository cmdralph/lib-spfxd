/* lib-spfxd — nftw / ftw: file tree walk with FTW_PHYS, FTW_DEPTH,
 * FTW_MOUNT and FTW_CHDIR. */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct walk {
	int (*fn)(const char *, const struct stat *, int, struct FTW *);
	int flags;
	dev_t dev;
	char path[PATH_MAX];
};

struct ancestor {
	dev_t dev;
	ino_t ino;
	const struct ancestor *up;
};

static int walk(struct walk *w, size_t len, int level, const struct ancestor *up)
{
	struct stat st;
	int type, r;
	size_t base = len;
	while (base && w->path[base - 1] != '/') base--;
	if (len > 1 && w->path[len - 1] == '/') base = len;

	int sr = (w->flags & FTW_PHYS) ? lstat(w->path, &st) : stat(w->path, &st);
	if (sr) {
		if (!(w->flags & FTW_PHYS) && errno == ENOENT && !lstat(w->path, &st)) type = FTW_SLN;
		else if (errno != EACCES) return -1;
		else type = FTW_NS;
	} else if (S_ISDIR(st.st_mode)) {
		type = access(w->path, R_OK) ? FTW_DNR : (w->flags & FTW_DEPTH) ? FTW_DP : FTW_D;
	} else if (S_ISLNK(st.st_mode)) {
		type = (w->flags & FTW_PHYS) ? FTW_SL : FTW_SLN;
	} else {
		type = FTW_F;
	}
	if ((w->flags & FTW_MOUNT) && level && type != FTW_NS && st.st_dev != w->dev) return 0;

	/* do not follow a directory cycle */
	if (type == FTW_D || type == FTW_DP) {
		for (const struct ancestor *a = up; a; a = a->up)
			if (a->dev == st.st_dev && a->ino == st.st_ino) return 0;
	}

	struct FTW ftw = { (int)base, level };
	if (type != FTW_DP && (r = w->fn(w->path, &st, type, &ftw))) return r;

	if (type == FTW_D || type == FTW_DP) {
		struct ancestor me = { st.st_dev, st.st_ino, up };
		DIR *d = opendir(w->path);
		if (d) {
			struct dirent *de;
			if (len && w->path[len - 1] != '/') w->path[len++] = '/';
			while ((de = readdir(d))) {
				if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
				size_t nl = strlen(de->d_name);
				if (len + nl >= sizeof w->path) {
					closedir(d);
					errno = ENAMETOOLONG;
					return -1;
				}
				memcpy(w->path + len, de->d_name, nl + 1);
				if ((r = walk(w, len + nl, level + 1, &me))) {
					closedir(d);
					return r;
				}
			}
			closedir(d);
			w->path[len] = 0;
			if (len > 1 && w->path[len - 1] == '/' && len - 1 >= base && base != len) w->path[--len] = 0;
		}
	}
	if (type == FTW_DP) {
		w->path[len] = 0;
		if ((r = w->fn(w->path, &st, type, &ftw))) return r;
	}
	return 0;
}

int nftw(const char *path, int (*fn)(const char *, const struct stat *, int, struct FTW *), int fds, int flags)
{
	struct walk w;
	size_t l = strlen(path);
	if (l >= sizeof w.path) {
		errno = ENAMETOOLONG;
		return -1;
	}
	w.fn = fn;
	w.flags = flags;
	memcpy(w.path, path, l + 1);
	struct stat st;
	w.dev = stat(path, &st) ? 0 : st.st_dev;
	int cwd = -1;
	if (flags & FTW_CHDIR) cwd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	int r = walk(&w, l, 0, 0);
	if (cwd >= 0) {
		if (fchdir(cwd)) r = r ? r : -1;
		close(cwd);
	}
	return r;
}

static int (*ftw_fn)(const char *, const struct stat *, int);
static int ftw_adapter(const char *p, const struct stat *st, int type, struct FTW *f)
{
	return ftw_fn(p, st, type == FTW_DP ? FTW_D : type);
}

int ftw(const char *path, int (*fn)(const char *, const struct stat *, int), int fds)
{
	ftw_fn = fn;
	return nftw(path, ftw_adapter, fds, FTW_PHYS);
}
