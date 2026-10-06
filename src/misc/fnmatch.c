/*
 * lib-spfxd — fnmatch: shell pattern matching (POSIX), with FNM_PATHNAME,
 * FNM_PERIOD, FNM_NOESCAPE, FNM_CASEFOLD and FNM_LEADING_DIR.
 *
 * '*' is matched with the classic backtracking scheme that remembers only
 * the most recent star, which is linear in practice and never recursive
 * (the last star can absorb whatever an earlier star would have).
 * Bracket expressions support ranges, negation with '!' or '^', and the
 * character classes [:alpha:] etc.
 */
#include <fnmatch.h>
#include <ctype.h>
#include <string.h>
#include <wctype.h>
#include <wchar.h>
#include <stdlib.h>

static int fold(int c, int flags)
{
	return (flags & FNM_CASEFOLD) ? tolower(c) : c;
}

/* Match one bracket expression at p (after '[') against c.  Returns the
 * pattern position after ']' through *end, or 0 if malformed. */
static int bracket(const char *p, int c, int flags, const char **end)
{
	int neg = 0, match = 0;
	if (*p == '!' || *p == '^') {
		neg = 1;
		p++;
	}
	const char *first = p;
	for (; *p && (*p != ']' || p == first); p++) {
		if (p[0] == '[' && p[1] == ':') {
			const char *close = strstr(p + 2, ":]");
			if (!close) return -1;
			char cls[16];
			size_t l = (size_t)(close - p - 2);
			if (l >= sizeof cls) return -1;
			memcpy(cls, p + 2, l);
			cls[l] = 0;
			wctype_t t = wctype(cls);
			if (!t) return -1;
			if (iswctype((wint_t)c, t)) match = 1;
			if ((flags & FNM_CASEFOLD) && (!strcmp(cls, "upper") || !strcmp(cls, "lower")) && isalpha(c))
				match = 1;
			p = close + 1;
			continue;
		}
		int lo = (unsigned char)*p;
		if (lo == '\\' && !(flags & FNM_NOESCAPE) && p[1]) lo = (unsigned char)*++p;
		int hi = lo;
		if (p[1] == '-' && p[2] && p[2] != ']') {
			const char *q = p + 2;
			hi = (unsigned char)*q;
			if (hi == '\\' && !(flags & FNM_NOESCAPE) && q[1]) hi = (unsigned char)*++q;
			p = q;
		}
		if (fold(c, flags) >= fold(lo, flags) && fold(c, flags) <= fold(hi, flags)) match = 1;
		else if (c >= lo && c <= hi) match = 1;
	}
	if (*p != ']') return -1;
	*end = p + 1;
	return match != neg;
}

static int match_seg(const char *p, const char *s, const char *s_end, int flags)
{
	const char *star_p = 0, *star_s = 0;
	const char *s0 = s;
	while (s < s_end || *p) {
		if (*p == '*') {
			/* a leading period must be matched explicitly */
			if ((flags & FNM_PERIOD) && s == s0 && s < s_end && *s == '.') return 0;
			while (*p == '*') p++;
			star_p = p;
			star_s = s;
			continue;
		}
		if (s < s_end && *p) {
			int c = (unsigned char)*s;
			const char *np = p + 1;
			int ok;
			if (*p == '?') {
				ok = !((flags & FNM_PERIOD) && s == s0 && c == '.');
			} else if (*p == '[') {
				const char *end;
				int r = bracket(p + 1, c, flags, &end);
				if (r < 0) ok = c == '[';   /* malformed: literal '[' */
				else {
					ok = r && !((flags & FNM_PERIOD) && s == s0 && c == '.');
					np = end;
				}
			} else {
				int pc = (unsigned char)*p;
				if (pc == '\\' && !(flags & FNM_NOESCAPE) && p[1]) {
					pc = (unsigned char)p[1];
					np = p + 2;
				}
				ok = fold(pc, flags) == fold(c, flags);
			}
			if (ok) {
				p = np;
				s++;
				continue;
			}
		}
		if (star_p && star_s < s_end) {
			p = star_p;
			s = ++star_s;
			continue;
		}
		return 0;
	}
	return 1;
}

int fnmatch(const char *pat, const char *str, int flags)
{
	if (!(flags & FNM_PATHNAME)) {
		const char *end = str + strlen(str);
		if (match_seg(pat, str, end, flags)) return 0;
		if (flags & FNM_LEADING_DIR) {
			for (const char *q = str; *q; q++)
				if (*q == '/' && match_seg(pat, str, q, flags)) return 0;
		}
		return FNM_NOMATCH;
	}
	/* FNM_PATHNAME: match component by component; '/' only matches '/' */
	for (;;) {
		const char *pe = pat;
		while (*pe && *pe != '/') {
			if (*pe == '[') {
				/* a bracket containing '/' cannot match; skip it whole */
				const char *q = pe + 1;
				if (*q == '!' || *q == '^') q++;
				if (*q == ']') q++;
				while (*q && *q != ']') q++;
				pe = *q ? q + 1 : pe + 1;
				continue;
			}
			if (*pe == '\\' && !(flags & FNM_NOESCAPE) && pe[1]) pe++;
			pe++;
		}
		const char *se = strchrnul(str, '/');
		size_t pl = (size_t)(pe - pat);
		char small[256], *seg = pl < sizeof small ? small : malloc(pl + 1);
		if (!seg) return FNM_NOMATCH;
		memcpy(seg, pat, pl);
		seg[pl] = 0;
		int ok = match_seg(seg, str, se, flags);
		if (seg != small) free(seg);
		if (!ok) return FNM_NOMATCH;
		if (!*pe) {
			if (!*se) return 0;
			return (flags & FNM_LEADING_DIR) ? 0 : FNM_NOMATCH;
		}
		if (!*se) return FNM_NOMATCH;
		pat = pe + 1;
		str = se + 1;
	}
}
