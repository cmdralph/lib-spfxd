/*
 * lib-spfxd — wordexp / wordfree.
 *
 * The words are first checked here: unquoted shell operators
 * (| & ; < > ( ) { } newline) give WRDE_BADCHAR and, with WRDE_NOCMD,
 * command substitution gives WRDE_CMDSUB.  The expansion itself (tilde,
 * parameter, command and arithmetic expansion, field splitting, pathname
 * expansion, quote removal) is then performed by /bin/sh, exactly as a
 * shell would, with
 *     eval "printf '%s\0' x $words"
 * and the NUL-separated fields read back through a pipe.
 */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>

/* Find the end of a $(...), $((...)) or ${...} starting at s (just past
 * the opening bracket), honouring quotes and nesting. */
static const char *skip_group(const char *s, char open, char close)
{
	int depth = 1;
	while (*s) {
		char c = *s++;
		if (c == '\\' && *s) { s++; continue; }
		if (c == '\'') { while (*s && *s != '\'') s++; if (*s) s++; continue; }
		if (c == '"') {
			while (*s && *s != '"') { if (*s == '\\' && s[1]) s++; s++; }
			if (*s) s++;
			continue;
		}
		if (c == open) depth++;
		else if (c == close && !--depth) return s;
	}
	return 0;
}

static int check(const char *s, int flags)
{
	int dq = 0;
	while (*s) {
		char c = *s++;
		switch (c) {
		case '\\':
			if (*s) s++;
			break;
		case '\'':
			if (dq) break;
			while (*s && *s != '\'') s++;
			if (!*s) return WRDE_SYNTAX;
			s++;
			break;
		case '"':
			dq = !dq;
			break;
		case '`':
			if (flags & WRDE_NOCMD) return WRDE_CMDSUB;
			while (*s && *s != '`') { if (*s == '\\' && s[1]) s++; s++; }
			if (!*s) return WRDE_SYNTAX;
			s++;
			break;
		case '$':
			if (*s == '(') {
				if (s[1] == '(') {
					const char *e = skip_group(s + 2, '(', ')');
					if (!e || *e != ')') return WRDE_SYNTAX;
					s = e + 1;
				} else {
					if (flags & WRDE_NOCMD) return WRDE_CMDSUB;
					const char *e = skip_group(s + 1, '(', ')');
					if (!e) return WRDE_SYNTAX;
					s = e;
				}
			} else if (*s == '{') {
				const char *e = skip_group(s + 1, '{', '}');
				if (!e) return WRDE_SYNTAX;
				s = e;
			}
			break;
		case '|': case '&': case ';': case '<': case '>': case '(': case ')':
		case '{': case '}': case '\n':
			if (!dq) return WRDE_BADCHAR;
			break;
		}
	}
	return dq ? WRDE_SYNTAX : 0;
}

void wordfree(wordexp_t *we)
{
	if (!we->we_wordv) return;
	for (size_t i = 0; i < we->we_wordc; i++) free(we->we_wordv[we->we_offs + i]);
	free(we->we_wordv);
	we->we_wordv = 0;
	we->we_wordc = 0;
}

int wordexp(const char *restrict s, wordexp_t *restrict we, int flags)
{
	int r = check(s, flags);
	if (r) return r;
	if (flags & WRDE_REUSE) wordfree(we);
	if (!(flags & WRDE_APPEND)) {
		we->we_wordc = 0;
		we->we_wordv = 0;
		if (!(flags & WRDE_DOOFFS)) we->we_offs = 0;
	}

	int p[2];
	if (pipe2(p, O_CLOEXEC) < 0) return WRDE_NOSPACE;
	const char *script = (flags & WRDE_UNDEF) ? "set -u; eval \"printf '%s\\\\0' x $1\""
	                                          : "eval \"printf '%s\\\\0' x $1\"";
	pid_t pid = fork();
	if (pid < 0) {
		close(p[0]);
		close(p[1]);
		return WRDE_NOSPACE;
	}
	if (!pid) {
		dup2(p[1], 1);
		if (!(flags & WRDE_SHOWERR)) {
			int fd = open("/dev/null", O_WRONLY);
			if (fd >= 0) dup2(fd, 2);
		}
		execl("/bin/sh", "sh", "-c", script, "sh", s, (char *)0);
		_exit(127);
	}
	close(p[1]);
	size_t cap = 4096, len = 0;
	char *buf = malloc(cap);
	ssize_t k;
	while (buf && (k = read(p[0], buf + len, cap - len)) != 0) {
		if (k < 0) {
			if (errno == EINTR) continue;
			break;
		}
		len += (size_t)k;
		if (len == cap) {
			char *nb = realloc(buf, cap * 2);
			if (!nb) { free(buf); buf = 0; break; }
			buf = nb;
			cap *= 2;
		}
	}
	close(p[0]);
	int status;
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR);
	if (!buf) return WRDE_NOSPACE;
	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		free(buf);
		return (flags & WRDE_UNDEF) ? WRDE_BADVAL : WRDE_SYNTAX;
	}
	/* fields: "x\0" then one per word */
	size_t n = 0;
	for (size_t i = 0; i < len; i++) if (!buf[i]) n++;
	if (n) n--;
	size_t offs = (flags & WRDE_DOOFFS) ? we->we_offs : 0;
	size_t old = we->we_wordc;
	char **v = realloc(we->we_wordv, (offs + old + n + 1) * sizeof *v);
	if (!v) {
		free(buf);
		return WRDE_NOSPACE;
	}
	if (!we->we_wordv)
		for (size_t i = 0; i < offs; i++) v[i] = 0;
	we->we_wordv = v;
	char *f = buf + strlen(buf) + 1;               /* skip the "x" */
	for (size_t i = 0; i < n; i++) {
		v[offs + old + i] = strdup(f);
		if (!v[offs + old + i]) {
			we->we_wordc = old + i;
			v[offs + old + i] = 0;
			free(buf);
			return WRDE_NOSPACE;
		}
		f += strlen(f) + 1;
	}
	we->we_wordc = old + n;
	v[offs + we->we_wordc] = 0;
	free(buf);
	return 0;
}
