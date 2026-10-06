/*
 * lib-spfxd — POSIX regular expressions: regcomp, regexec, regerror,
 * regfree.
 *
 * Patterns (basic or extended syntax, with the common GNU extensions
 * \+ \? \| in BREs, back-references in EREs, \w \W \s \S \b \B \< \>) are
 * parsed into a tree and compiled to a small instruction set:
 *
 *   CHAR c   ANY   ANYNL   SET i      consume one character
 *   BOL EOL WORDB NWORDB WBEG WEND     zero-width assertions
 *   SAVE k   SPLIT x,y   JMP x         control (SPLIT prefers x)
 *   MARK k   CHKPROG k                 empty-loop guard (backtracking)
 *   BREF n   MATCH
 *
 * Matching semantics: the leftmost match is reported, and among matches
 * starting there the longest (POSIX).  Without back-references a Pike VM
 * runs all alternatives in lock step: time O(length x program), no
 * exponential blowup.  Sub-match positions are those of the
 * highest-priority thread (greedy repetition, earlier alternatives first)
 * that produces the leftmost-longest match.  Patterns with
 * back-references need backtracking; that engine searches exhaustively
 * for the longest match at each start position and gives up with
 * REG_ESPACE after a fixed amount of work.
 *
 * Characters are bytes in the C locale and Unicode code points in a UTF-8
 * locale (invalid bytes in the subject match only '.' and negated sets).
 */
#include <ctype.h>
#include <limits.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "locale_impl.h"

#define RE_DUP_MAX_ 32767
#define MAX_PROG 200000
#define BT_BUDGET 20000000UL
#define BAD_BYTE 0x110000                /* + byte: an undecodable byte */

/* ------------------------------------------------------------ sets */

struct range { wint_t lo, hi; };

struct cset {
	unsigned char bits[32];          /* members below 256 */
	struct range *ranges;            /* members >= 256 (and folded ranges) */
	size_t nranges;
	wctype_t *classes;
	size_t nclasses;
	unsigned char neg, icase, nonl;
};

static int set_add_range(struct cset *s, wint_t lo, wint_t hi)
{
	for (wint_t c = lo; c <= hi && c < 256; c++) s->bits[c >> 3] |= (unsigned char)(1u << (c & 7));
	if (hi >= 256) {
		struct range *r = realloc(s->ranges, (s->nranges + 1) * sizeof *r);
		if (!r) return -1;
		s->ranges = r;
		s->ranges[s->nranges++] = (struct range){ lo < 256 ? 256 : lo, hi };
	}
	return 0;
}

static int set_raw(const struct cset *s, wint_t c)
{
	if (c < 256) {
		if (s->bits[c >> 3] & (1u << (c & 7))) return 1;
	} else {
		for (size_t i = 0; i < s->nranges; i++)
			if (c >= s->ranges[i].lo && c <= s->ranges[i].hi) return 1;
	}
	for (size_t i = 0; i < s->nclasses; i++)
		if (iswctype(c, s->classes[i])) return 1;
	return 0;
}

static int set_match(const struct cset *s, int c)
{
	int in;
	if (c >= BAD_BYTE) {
		in = 0;
	} else {
		in = set_raw(s, (wint_t)c);
		if (!in && s->icase)
			in = set_raw(s, towlower((wint_t)c)) || set_raw(s, towupper((wint_t)c));
	}
	if (s->neg) {
		if (s->nonl && c == '\n') return 0;
		return !in;
	}
	return in;
}

/* ---------------------------------------------------------- program */

enum {
	I_CHAR, I_ANY, I_ANYNL, I_SET, I_BOL, I_EOL, I_WORDB, I_NWORDB, I_WBEG, I_WEND,
	I_SAVE, I_SPLIT, I_JMP, I_MARK, I_CHKPROG, I_BREF, I_MATCH,
};

struct inst {
	unsigned char op;
	int x, y;
};

struct prog {
	struct inst *code;
	size_t n, cap;
	struct cset *sets;
	size_t nsets;
	size_t nsub;
	size_t nmarks;
	int has_bref;
	int icase, newline, utf8;
	int anchored;                    /* starts with BOL (and not NEWLINE) */
};

/* ------------------------------------------------------------- parse */

enum { N_CHAR, N_ANY, N_SET, N_BOL, N_EOL, N_WORDB, N_NWORDB, N_WBEG, N_WEND,
       N_GROUP, N_CAT, N_ALT, N_REP, N_BREF, N_EMPTY };

struct node {
	int type;
	int a;                       /* char, set index, group number */
	int min, max;                /* repetition; max -1 = unbounded */
	int l, r;                    /* children */
};

struct parser {
	const unsigned char *s;
	int ere, icase, newline, utf8;
	int err;
	struct node *nodes;
	size_t nn, ncap;
	struct prog *p;
	int ngroups;
	unsigned char closed[10];    /* groups 1..9 completed (for \n checks) */
	int depth;
};

static int new_node(struct parser *ps, int type)
{
	if (ps->nn == ps->ncap) {
		size_t nc = ps->ncap ? 2 * ps->ncap : 64;
		struct node *n = realloc(ps->nodes, nc * sizeof *n);
		if (!n) {
			ps->err = REG_ESPACE;
			return -1;
		}
		ps->nodes = n;
		ps->ncap = nc;
	}
	memset(&ps->nodes[ps->nn], 0, sizeof *ps->nodes);
	ps->nodes[ps->nn].type = type;
	ps->nodes[ps->nn].l = ps->nodes[ps->nn].r = -1;
	return (int)ps->nn++;
}

static int bin_node(struct parser *ps, int type, int l, int r)
{
	if (l < 0) return r;
	if (r < 0) return l;
	int n = new_node(ps, type);
	if (n >= 0) {
		ps->nodes[n].l = l;
		ps->nodes[n].r = r;
	}
	return n;
}

/* Decode one pattern character (UTF-8 aware); returns its code point. */
static int pat_char(struct parser *ps, int *len)
{
	const unsigned char *s = ps->s;
	if (!ps->utf8 || *s < 0x80) {
		*len = 1;
		return *s;
	}
	wchar_t wc;
	mbstate_t st;
	memset(&st, 0, sizeof st);
	size_t l = mbrtowc(&wc, (const char *)s, 4, &st);
	if (l == (size_t)-1 || l == (size_t)-2 || !l) {
		*len = 1;
		return BAD_BYTE + *s;
	}
	*len = (int)l;
	return (int)wc;
}

static int new_set(struct parser *ps)
{
	struct prog *p = ps->p;
	struct cset *ns = realloc(p->sets, (p->nsets + 1) * sizeof *ns);
	if (!ns) {
		ps->err = REG_ESPACE;
		return -1;
	}
	p->sets = ns;
	memset(&ns[p->nsets], 0, sizeof *ns);
	ns[p->nsets].icase = (unsigned char)ps->icase;
	return (int)p->nsets++;
}

static int add_class(struct parser *ps, struct cset *s, const char *name)
{
	wctype_t t = wctype(name);
	if (!t) {
		ps->err = REG_ECTYPE;
		return -1;
	}
	wctype_t *c = realloc(s->classes, (s->nclasses + 1) * sizeof *c);
	if (!c) {
		ps->err = REG_ESPACE;
		return -1;
	}
	s->classes = c;
	s->classes[s->nclasses++] = t;
	return 0;
}

/* One element of a bracket expression that can be a range endpoint:
 * an ordinary character, [.c.] or [=c=] (single characters only). */
static int bracket_char(struct parser *ps, int *is_equiv)
{
	int len;
	*is_equiv = 0;
	if (ps->s[0] == '[' && (ps->s[1] == '.' || ps->s[1] == '=')) {
		unsigned char kind = ps->s[1];
		ps->s += 2;
		int c = pat_char(ps, &len);
		ps->s += len;
		if (ps->s[0] != kind || ps->s[1] != ']') {
			ps->err = REG_ECOLLATE;
			return -1;
		}
		ps->s += 2;
		*is_equiv = kind == '=';
		return c;
	}
	int c = pat_char(ps, &len);
	ps->s += len;
	return c;
}

static int parse_bracket(struct parser *ps)
{
	int si = new_set(ps);
	if (si < 0) return -1;
	struct cset *s = &ps->p->sets[si];
	if (*ps->s == '^') {
		s->neg = 1;
		s->nonl = (unsigned char)ps->newline;
		ps->s++;
	}
	int first = 1;
	for (;;) {
		if (!*ps->s) {
			ps->err = REG_EBRACK;
			return -1;
		}
		if (*ps->s == ']' && !first) {
			ps->s++;
			break;
		}
		first = 0;
		if (ps->s[0] == '[' && ps->s[1] == ':') {
			const unsigned char *e = (const unsigned char *)strstr((const char *)ps->s + 2, ":]");
			if (!e || e - ps->s - 2 > 15) {
				ps->err = REG_ECTYPE;
				return -1;
			}
			char name[16];
			memcpy(name, ps->s + 2, (size_t)(e - ps->s - 2));
			name[e - ps->s - 2] = 0;
			s = &ps->p->sets[si];
			if (add_class(ps, s, name) < 0) return -1;
			ps->s = e + 2;
			continue;
		}
		int eq;
		int lo = bracket_char(ps, &eq);
		if (lo < 0) return -1;
		int hi = lo;
		if (ps->s[0] == '-' && ps->s[1] != ']' && ps->s[1] && !eq) {
			ps->s++;
			hi = bracket_char(ps, &eq);
			if (hi < 0) return -1;
			if (hi < lo || eq) {
				ps->err = REG_ERANGE;
				return -1;
			}
		}
		s = &ps->p->sets[si];
		if (lo >= BAD_BYTE) continue;    /* invalid pattern bytes never match */
		if (set_add_range(s, (wint_t)lo, (wint_t)hi) < 0) {
			ps->err = REG_ESPACE;
			return -1;
		}
		if (ps->icase && lo == hi) {
			wint_t u = towupper((wint_t)lo), l = towlower((wint_t)lo);
			set_add_range(s, u, u);
			set_add_range(s, l, l);
		}
	}
	int n = new_node(ps, N_SET);
	if (n >= 0) ps->nodes[n].a = si;
	return n;
}

static int parse_alt(struct parser *ps);

static int char_node(struct parser *ps, int c)
{
	int n = new_node(ps, N_CHAR);
	if (n >= 0) ps->nodes[n].a = ps->icase && c < BAD_BYTE ? (int)towlower((wint_t)c) : c;
	return n;
}

static int class_node(struct parser *ps, const char *cls, int neg)
{
	int si = new_set(ps);
	if (si < 0) return -1;
	struct cset *s = &ps->p->sets[si];
	if (add_class(ps, s, cls) < 0) return -1;
	s = &ps->p->sets[si];
	if (!strcmp(cls, "alnum")) set_add_range(s, '_', '_');
	s->neg = (unsigned char)neg;
	int n = new_node(ps, N_SET);
	if (n >= 0) ps->nodes[n].a = si;
	return n;
}

/* Is the BRE position at the end of a subexpression (for '$')? */
static int bre_at_end(const unsigned char *s)
{
	return !s[0] || (s[0] == '\\' && (s[1] == ')' || s[1] == '|'));
}

static int parse_atom(struct parser *ps, int at_start)
{
	const unsigned char *s = ps->s;
	int len;
	if (ps->ere) {
		switch (*s) {
		case '(': {
			ps->s++;
			int g = ++ps->ngroups;
			if (++ps->depth > 1000) { ps->err = REG_ESPACE; return -1; }
			int inner = parse_alt(ps);
			ps->depth--;
			if (ps->err) return -1;
			if (*ps->s != ')') {
				ps->err = REG_EPAREN;
				return -1;
			}
			ps->s++;
			if (g < 10) ps->closed[g] = 1;
			int n = new_node(ps, N_GROUP);
			if (n >= 0) {
				ps->nodes[n].a = g;
				ps->nodes[n].l = inner < 0 ? new_node(ps, N_EMPTY) : inner;
			}
			return n;
		}
		case '.': ps->s++; return new_node(ps, N_ANY);
		case '[': ps->s++; return parse_bracket(ps);
		case '^': ps->s++; return new_node(ps, N_BOL);
		case '$': ps->s++; return new_node(ps, N_EOL);
		case '*': case '+': case '?':
			ps->err = REG_BADRPT;
			return -1;
		case '{':
			if (s[1] >= '0' && s[1] <= '9') {
				ps->err = REG_BADRPT;
				return -1;
			}
			ps->s++;
			return char_node(ps, '{');
		case '\\':
			break;
		default: {
			int c = pat_char(ps, &len);
			ps->s += len;
			return char_node(ps, c);
		}
		}
	} else {
		switch (*s) {
		case '.': ps->s++; return new_node(ps, N_ANY);
		case '[': ps->s++; return parse_bracket(ps);
		case '^':
			ps->s++;
			if (at_start) return new_node(ps, N_BOL);
			return char_node(ps, '^');
		case '$':
			ps->s++;
			if (bre_at_end(ps->s)) return new_node(ps, N_EOL);
			return char_node(ps, '$');
		case '*':
			/* literal at the start of an expression */
			ps->s++;
			return char_node(ps, '*');
		case '\\':
			break;
		default: {
			int c = pat_char(ps, &len);
			ps->s += len;
			return char_node(ps, c);
		}
		}
	}
	/* backslash escapes */
	s = ++ps->s;
	if (!*s) {
		ps->err = REG_EESCAPE;
		return -1;
	}
	if (!ps->ere && *s == '(') {
		ps->s++;
		int g = ++ps->ngroups;
		if (++ps->depth > 1000) { ps->err = REG_ESPACE; return -1; }
		int inner = parse_alt(ps);
		ps->depth--;
		if (ps->err) return -1;
		if (ps->s[0] != '\\' || ps->s[1] != ')') {
			ps->err = REG_EPAREN;
			return -1;
		}
		ps->s += 2;
		if (g < 10) ps->closed[g] = 1;
		int n = new_node(ps, N_GROUP);
		if (n >= 0) {
			ps->nodes[n].a = g;
			ps->nodes[n].l = inner < 0 ? new_node(ps, N_EMPTY) : inner;
		}
		return n;
	}
	if (*s >= '1' && *s <= '9') {
		int g = *s - '0';
		if (g > ps->ngroups || !ps->closed[g]) {
			ps->err = REG_ESUBREG;
			return -1;
		}
		ps->s++;
		ps->p->has_bref = 1;
		int n = new_node(ps, N_BREF);
		if (n >= 0) ps->nodes[n].a = g;
		return n;
	}
	ps->s++;
	switch (*s) {
	case 'w': return class_node(ps, "alnum", 0);
	case 'W': return class_node(ps, "alnum", 1);
	case 's': return class_node(ps, "space", 0);
	case 'S': return class_node(ps, "space", 1);
	case 'b': return new_node(ps, N_WORDB);
	case 'B': return new_node(ps, N_NWORDB);
	case '<': return new_node(ps, N_WBEG);
	case '>': return new_node(ps, N_WEND);
	case '`': return new_node(ps, N_BOL);
	case '\'': return new_node(ps, N_EOL);
	case 'n': return char_node(ps, '\n');
	case 't': return char_node(ps, '\t');
	default:
		if (!ps->ere && (*s == '{' || *s == '}' || *s == ')')) {
			ps->err = *s == ')' ? REG_EPAREN : REG_BADRPT;
			return -1;
		}
		ps->s = s;
		{
			int c = pat_char(ps, &len);
			ps->s += len;
			return char_node(ps, c);
		}
	}
}

static int parse_count(struct parser *ps, int *v)
{
	if (*ps->s < '0' || *ps->s > '9') return 0;
	long n = 0;
	while (*ps->s >= '0' && *ps->s <= '9') {
		n = n * 10 + (*ps->s++ - '0');
		if (n > RE_DUP_MAX_) {
			ps->err = REG_BADBR;
			return -1;
		}
	}
	*v = (int)n;
	return 1;
}

/* Parse a quantifier if one follows; returns 1 with min/max set. */
static int parse_quant(struct parser *ps, int *min, int *max)
{
	const unsigned char *s = ps->s;
	if (ps->ere) {
		if (*s == '*') { ps->s++; *min = 0; *max = -1; return 1; }
		if (*s == '+') { ps->s++; *min = 1; *max = -1; return 1; }
		if (*s == '?') { ps->s++; *min = 0; *max = 1; return 1; }
		if (*s == '{' && s[1] >= '0' && s[1] <= '9') {
			ps->s++;
		} else if (*s == '{' && s[1] == ',') {
			ps->s++;
		} else {
			return 0;
		}
	} else {
		if (*s == '*') { ps->s++; *min = 0; *max = -1; return 1; }
		if (*s == '\\' && s[1] == '+') { ps->s += 2; *min = 1; *max = -1; return 1; }
		if (*s == '\\' && s[1] == '?') { ps->s += 2; *min = 0; *max = 1; return 1; }
		if (*s == '\\' && s[1] == '{') ps->s += 2;
		else return 0;
	}
	/* interval: {m}, {m,}, {m,n}, {,n} */
	int lo = 0, hi;
	if (parse_count(ps, &lo) < 0) return -1;
	hi = lo;
	if (*ps->s == ',') {
		ps->s++;
		hi = -1;
		if (parse_count(ps, &hi) < 0) return -1;
	}
	if (ps->ere) {
		if (*ps->s != '}') { ps->err = REG_EBRACE; return -1; }
		ps->s++;
	} else {
		if (ps->s[0] != '\\' || ps->s[1] != '}') { ps->err = REG_EBRACE; return -1; }
		ps->s += 2;
	}
	if (hi != -1 && hi < lo) {
		ps->err = REG_BADBR;
		return -1;
	}
	*min = lo;
	*max = hi;
	return 1;
}

static int parse_rep(struct parser *ps, int at_start)
{
	int n = parse_atom(ps, at_start);
	if (n < 0) return -1;
	int t = ps->nodes[n].type;
	if (!ps->ere && t == N_BOL) return n;   /* "^*": the star is literal */
	for (;;) {
		int min, max;
		int q = parse_quant(ps, &min, &max);
		if (q < 0) return -1;
		if (!q) break;
		if ((t == N_BOL || t == N_EOL) && ps->ere) {
			ps->err = REG_BADRPT;
			return -1;
		}
		if (t == N_REP && !ps->ere) {
			/* "a**" and similar: rejected in basic syntax */
			ps->err = REG_BADRPT;
			return -1;
		}
		int r = new_node(ps, N_REP);
		if (r < 0) return -1;
		ps->nodes[r].l = n;
		ps->nodes[r].min = min;
		ps->nodes[r].max = max;
		n = r;
		t = N_REP;
	}
	return n;
}

static int at_alt_end(struct parser *ps)
{
	const unsigned char *s = ps->s;
	if (!*s) return 1;
	if (ps->ere) return *s == '|' || (*s == ')' && ps->depth > 0);
	return s[0] == '\\' && (s[1] == '|' || (s[1] == ')' && ps->depth > 0));
}

static int parse_cat(struct parser *ps)
{
	int n = -1, first = 1;
	while (!at_alt_end(ps)) {
		int a = parse_rep(ps, first);
		if (a < 0) return -1;
		n = n < 0 ? a : bin_node(ps, N_CAT, n, a);
		if (n < 0) return -1;
		first = 0;
	}
	return n < 0 ? new_node(ps, N_EMPTY) : n;
}

static int parse_alt(struct parser *ps)
{
	int n = parse_cat(ps);
	if (n < 0) return -1;
	for (;;) {
		if (ps->ere && *ps->s == '|') ps->s++;
		else if (!ps->ere && ps->s[0] == '\\' && ps->s[1] == '|') ps->s += 2;
		else break;
		int m = parse_cat(ps);
		if (m < 0) return -1;
		n = bin_node(ps, N_ALT, n, m);
		if (n < 0) return -1;
	}
	return n;
}

/* ----------------------------------------------------------- compile */

static int emit(struct prog *p, int op, int x, int y)
{
	if (p->n == p->cap) {
		if (p->cap >= MAX_PROG) return -1;
		size_t nc = p->cap ? 2 * p->cap : 64;
		struct inst *c = realloc(p->code, nc * sizeof *c);
		if (!c) return -1;
		p->code = c;
		p->cap = nc;
	}
	p->code[p->n] = (struct inst){ (unsigned char)op, x, y };
	return (int)p->n++;
}

static int nullable(struct parser *ps, int n)
{
	struct node *nd = &ps->nodes[n];
	switch (nd->type) {
	case N_CHAR: case N_ANY: case N_SET: return 0;
	case N_CAT: return nullable(ps, nd->l) && nullable(ps, nd->r);
	case N_ALT: return nullable(ps, nd->l) || nullable(ps, nd->r);
	case N_GROUP: return nullable(ps, nd->l);
	case N_REP: return nd->min == 0 || nullable(ps, nd->l);
	default: return 1;
	}
}

static int compile(struct parser *ps, int n)
{
	struct prog *p = ps->p;
	struct node *nd = &ps->nodes[n];
	int a, b;
	switch (nd->type) {
	case N_CHAR: return emit(p, I_CHAR, nd->a, 0) < 0 ? -1 : 0;
	case N_ANY: return emit(p, ps->newline ? I_ANYNL : I_ANY, 0, 0) < 0 ? -1 : 0;
	case N_SET: return emit(p, I_SET, nd->a, 0) < 0 ? -1 : 0;
	case N_BOL: return emit(p, I_BOL, 0, 0) < 0 ? -1 : 0;
	case N_EOL: return emit(p, I_EOL, 0, 0) < 0 ? -1 : 0;
	case N_WORDB: return emit(p, I_WORDB, 0, 0) < 0 ? -1 : 0;
	case N_NWORDB: return emit(p, I_NWORDB, 0, 0) < 0 ? -1 : 0;
	case N_WBEG: return emit(p, I_WBEG, 0, 0) < 0 ? -1 : 0;
	case N_WEND: return emit(p, I_WEND, 0, 0) < 0 ? -1 : 0;
	case N_BREF: return emit(p, I_BREF, nd->a, 0) < 0 ? -1 : 0;
	case N_EMPTY: return 0;
	case N_GROUP: {
		int g = nd->a, child = nd->l;
		if (emit(p, I_SAVE, 2 * g, 0) < 0) return -1;
		if (compile(ps, child) < 0) return -1;
		return emit(p, I_SAVE, 2 * g + 1, 0) < 0 ? -1 : 0;
	}
	case N_CAT: {
		int l = nd->l, r = nd->r;
		if (compile(ps, l) < 0) return -1;
		return compile(ps, r);
	}
	case N_ALT: {
		int l = nd->l, r = nd->r;
		a = emit(p, I_SPLIT, 0, 0);
		if (a < 0) return -1;
		p->code[a].x = (int)p->n;
		if (compile(ps, l) < 0) return -1;
		b = emit(p, I_JMP, 0, 0);
		if (b < 0) return -1;
		p->code[a].y = (int)p->n;
		if (compile(ps, r) < 0) return -1;
		p->code[b].x = (int)p->n;
		return 0;
	}
	case N_REP: {
		int child = nd->l, min = nd->min, max = nd->max;
		int nul = nullable(ps, child);
		for (int i = 0; i < min; i++)
			if (compile(ps, child) < 0) return -1;
		if (max == -1) {
			/* L: SPLIT body, out; body: [MARK] child [CHKPROG] JMP L */
			int k = nul ? (int)p->nmarks++ : -1;
			int L = emit(p, I_SPLIT, 0, 0);
			if (L < 0) return -1;
			p->code[L].x = (int)p->n;
			if (k >= 0 && emit(p, I_MARK, k, 0) < 0) return -1;
			if (compile(ps, child) < 0) return -1;
			if (k >= 0 && emit(p, I_CHKPROG, k, 0) < 0) return -1;
			if (emit(p, I_JMP, L, 0) < 0) return -1;
			p->code[L].y = (int)p->n;
			return 0;
		}
		/* (max - min) nested optional copies */
		int opt = max - min;
		int *splits = opt ? malloc((size_t)opt * sizeof *splits) : 0;
		if (opt && !splits) return -1;
		for (int i = 0; i < opt; i++) {
			splits[i] = emit(p, I_SPLIT, 0, 0);
			if (splits[i] < 0 || compile(ps, child) < 0) {
				free(splits);
				return -1;
			}
		}
		for (int i = 0; i < opt; i++) {
			p->code[splits[i]].x = splits[i] + 1;
			p->code[splits[i]].y = (int)p->n;
		}
		free(splits);
		return 0;
	}
	}
	return -1;
}

static void free_prog(struct prog *p)
{
	if (!p) return;
	for (size_t i = 0; i < p->nsets; i++) {
		free(p->sets[i].ranges);
		free(p->sets[i].classes);
	}
	free(p->sets);
	free(p->code);
	free(p);
}

int regcomp(regex_t *restrict re, const char *restrict pat, int cflags)
{
	struct prog *p = calloc(1, sizeof *p);
	if (!p) return REG_ESPACE;
	struct parser ps;
	memset(&ps, 0, sizeof ps);
	ps.s = (const unsigned char *)pat;
	ps.ere = !!(cflags & REG_EXTENDED);
	ps.icase = !!(cflags & REG_ICASE);
	ps.newline = !!(cflags & REG_NEWLINE);
	ps.utf8 = __locale_utf8();
	ps.p = p;
	p->icase = ps.icase;
	p->newline = ps.newline;
	p->utf8 = ps.utf8;

	int root = parse_alt(&ps);
	if (!ps.err && *ps.s) ps.err = REG_EPAREN;
	if (ps.err || root < 0) {
		int e = ps.err ? ps.err : REG_ESPACE;
		free(ps.nodes);
		free_prog(p);
		return e;
	}
	p->nsub = (size_t)ps.ngroups;
	p->anchored = ps.nodes[root].type == N_BOL && !ps.newline;
	if (emit(p, I_SAVE, 0, 0) < 0 || compile(&ps, root) < 0 ||
	    emit(p, I_SAVE, 1, 0) < 0 || emit(p, I_MATCH, 0, 0) < 0) {
		free(ps.nodes);
		free_prog(p);
		return REG_ESPACE;
	}
	if (ps.nn && ps.nodes[root].type == N_CAT) {
		int l = root;
		while (ps.nodes[l].type == N_CAT) l = ps.nodes[l].l;
		if (ps.nodes[l].type == N_BOL && !ps.newline) p->anchored = 1;
	}
	free(ps.nodes);
	re->re_nsub = p->nsub;
	re->__prog = p;
	re->__cflags = cflags;
	return 0;
}

void regfree(regex_t *re)
{
	free_prog(re->__prog);
	re->__prog = 0;
}

/* ------------------------------------------------------------ match */

struct subject {
	const unsigned char *s;          /* string start (offsets are relative to it) */
	size_t start, end;               /* search range */
	int utf8, notbol, noteol, icase, newline;
};

/* Character at byte offset i (< end); *len receives its length. */
static int char_at(const struct subject *sj, size_t i, int *len)
{
	unsigned c = sj->s[i];
	if (!sj->utf8 || c < 0x80) {
		*len = 1;
		return (int)c;
	}
	/* strict UTF-8 decode */
	size_t avail = sj->end - i;
	int n;
	unsigned min;
	if (c >= 0xc2 && c <= 0xdf) { n = 2; c &= 0x1f; min = 0x80; }
	else if (c >= 0xe0 && c <= 0xef) { n = 3; c &= 0x0f; min = 0x800; }
	else if (c >= 0xf0 && c <= 0xf4) { n = 4; c &= 0x07; min = 0x10000; }
	else goto bad;
	if ((size_t)n > avail) goto bad;
	for (int k = 1; k < n; k++) {
		unsigned b = sj->s[i + (size_t)k];
		if ((b & 0xc0) != 0x80) goto bad;
		c = c << 6 | (b & 0x3f);
	}
	if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) goto bad;
	*len = n;
	return (int)c;
bad:
	*len = 1;
	return BAD_BYTE + sj->s[i];
}

static int is_word(int c)
{
	return c == '_' || (c >= 0 && c < BAD_BYTE && iswalnum((wint_t)c));
}

/* Context for zero-width assertions at byte offset i. */
static int assert_ok(const struct subject *sj, int op, size_t i, int prev)
{
	int len, next = i < sj->end ? char_at(sj, i, &len) : -1;
	switch (op) {
	case I_BOL:
		if (i == sj->start && !sj->notbol) return 1;
		return sj->newline && prev == '\n';
	case I_EOL:
		if (i == sj->end && !sj->noteol) return 1;
		return sj->newline && next == '\n';
	default: {
		int pw = prev >= 0 && is_word(prev), nw = next >= 0 && is_word(next);
		switch (op) {
		case I_WORDB: return pw != nw;
		case I_NWORDB: return pw == nw;
		case I_WBEG: return !pw && nw;
		case I_WEND: return pw && !nw;
		}
	}
	}
	return 0;
}

static int char_ok(const struct prog *p, const struct inst *in, int c)
{
	switch (in->op) {
	case I_CHAR:
		if (in->x == c) return 1;
		return p->icase && c < BAD_BYTE && (int)towlower((wint_t)c) == in->x;
	case I_ANY: return 1;
	case I_ANYNL: return c != '\n';
	case I_SET: return set_match(&p->sets[in->x], c);
	}
	return 0;
}

/* ---- Pike VM ---- */

struct tlist {
	size_t n;
	int *pc;
	regoff_t *caps;                  /* n x ncap */
};

struct vm {
	const struct prog *p;
	const struct subject *sj;
	size_t ncap;
	unsigned *mark;                  /* generation stamp per instruction */
	unsigned gen;
	size_t *stk;                     /* closure stack: pc | (undo << 63) ... */
	regoff_t *stkval;
	regoff_t *cur;
};

#define UNDO_FLAG ((size_t)1 << 62)

/* Add the epsilon closure of pc to list l, with captures `caps`, at
 * offset i.  Higher-priority paths are added first. */
static void addthread(struct vm *vm, struct tlist *l, int pc0, const regoff_t *caps, size_t i, int prev)
{
	const struct prog *p = vm->p;
	size_t sp = 0, ncap = vm->ncap;
	regoff_t *cur = vm->cur;
	memcpy(cur, caps, ncap * sizeof *cur);
	vm->stk[sp++] = (size_t)pc0;
	while (sp) {
		size_t e = vm->stk[--sp];
		if (e & UNDO_FLAG) {
			cur[e & ~UNDO_FLAG] = vm->stkval[sp];
			continue;
		}
		int pc = (int)e;
		if (vm->mark[pc] == vm->gen) continue;
		vm->mark[pc] = vm->gen;
		const struct inst *in = &p->code[pc];
		switch (in->op) {
		case I_JMP:
			vm->stk[sp++] = (size_t)in->x;
			break;
		case I_SPLIT:
			vm->stk[sp++] = (size_t)in->y;
			vm->stk[sp++] = (size_t)in->x;
			break;
		case I_SAVE:
			if ((size_t)in->x < ncap) {
				vm->stkval[sp] = cur[in->x];
				vm->stk[sp++] = UNDO_FLAG | (size_t)in->x;
				cur[in->x] = (regoff_t)i;
			}
			vm->stk[sp++] = (size_t)pc + 1;
			break;
		case I_MARK:
		case I_CHKPROG:
			vm->stk[sp++] = (size_t)pc + 1;
			break;
		case I_BOL: case I_EOL: case I_WORDB: case I_NWORDB: case I_WBEG: case I_WEND:
			if (assert_ok(vm->sj, in->op, i, prev)) vm->stk[sp++] = (size_t)pc + 1;
			break;
		default:
			l->pc[l->n] = pc;
			memcpy(l->caps + l->n * ncap, cur, ncap * sizeof *cur);
			l->n++;
			break;
		}
	}
}

static int pike(const struct prog *p, const struct subject *sj, size_t ncap, regoff_t *out)
{
	size_t n = p->n;
	struct vm vm;
	memset(&vm, 0, sizeof vm);
	vm.p = p;
	vm.sj = sj;
	vm.ncap = ncap;
	struct tlist a, b;
	size_t bytes = 2 * n * sizeof(int) + 2 * n * ncap * sizeof(regoff_t) + n * sizeof(unsigned) +
	               3 * n * sizeof(size_t) + 3 * n * sizeof(regoff_t) + 3 * ncap * sizeof(regoff_t);
	char *mem = malloc(bytes);
	if (!mem) return REG_ESPACE;
	char *m = mem;
	a.pc = (int *)m; m += n * sizeof(int);
	b.pc = (int *)m; m += n * sizeof(int);
	a.caps = (regoff_t *)m; m += n * ncap * sizeof(regoff_t);
	b.caps = (regoff_t *)m; m += n * ncap * sizeof(regoff_t);
	vm.mark = (unsigned *)m; m += n * sizeof(unsigned);
	vm.stk = (size_t *)m; m += 3 * n * sizeof(size_t);
	vm.stkval = (regoff_t *)m; m += 3 * n * sizeof(regoff_t);
	vm.cur = (regoff_t *)m; m += ncap * sizeof(regoff_t);
	regoff_t *seed = (regoff_t *)m; m += ncap * sizeof(regoff_t);
	regoff_t *best = (regoff_t *)m;
	memset(vm.mark, 0, n * sizeof(unsigned));
	for (size_t k = 0; k < ncap; k++) seed[k] = -1;

	struct tlist *cl = &a, *nl = &b;
	cl->n = nl->n = 0;
	int matched = 0;
	regoff_t best_so = -1, best_eo = -1;
	int prev = -1;
	if (sj->start > 0) {
		/* previous character for assertions: step back over UTF-8 */
		size_t k = sj->start - 1;
		if (sj->utf8)
			while (k > 0 && sj->start - k < 4 && (sj->s[k] & 0xc0) == 0x80) k--;
		int len;
		struct subject tmp = *sj;
		tmp.end = sj->start;
		prev = char_at(&tmp, k, &len);
	}
	size_t i = sj->start;
	for (;;) {
		vm.gen++;
		if (!matched && (!p->anchored || i == sj->start)) addthread(&vm, cl, 0, seed, i, prev);
		if (!cl->n && (matched || (p->anchored && i > sj->start))) break;
		int len = 1, c = -1;
		if (i < sj->end) c = char_at(sj, i, &len);
		vm.gen++;
		nl->n = 0;
		for (size_t t = 0; t < cl->n; t++) {
			int pc = cl->pc[t];
			regoff_t *caps = cl->caps + t * ncap;
			const struct inst *in = &p->code[pc];
			if (matched && caps[0] > best_so) continue;   /* starts too late */
			if (in->op == I_MATCH) {
				if (!matched || caps[0] < best_so || (caps[0] == best_so && caps[1] > best_eo)) {
					matched = 1;
					best_so = caps[0];
					best_eo = caps[1];
					memcpy(best, caps, ncap * sizeof *best);
				}
				continue;
			}
			if (c >= 0 && char_ok(p, in, c)) addthread(&vm, nl, pc + 1, caps, i + (size_t)len, c);
		}
		struct tlist *tmp = cl;
		cl = nl;
		nl = tmp;
		if (i >= sj->end) break;    /* nothing can be consumed past the end */
		prev = c;
		i += (size_t)len;
	}
	if (matched) memcpy(out, best, ncap * sizeof *out);
	free(mem);
	return matched ? 0 : REG_NOMATCH;
}

/* ---- backtracking (back-references) ---- */

struct btframe {
	int pc;
	size_t i;
	int prev;
	size_t undo;                     /* undo-log height to restore */
};

struct undo { int slot; regoff_t val; };

static int backtrack(const struct prog *p, const struct subject *sj, size_t ncap_out, regoff_t *out)
{
	size_t ncap = 2 * (p->nsub + 1), nslots = ncap + p->nmarks;
	regoff_t *cur = malloc(nslots * sizeof *cur);
	regoff_t *best = malloc(ncap * sizeof *best);
	size_t fcap = 256, ucap = 256;
	struct btframe *fs = malloc(fcap * sizeof *fs);
	struct undo *us = malloc(ucap * sizeof *us);
	int ret = REG_NOMATCH;
	unsigned long steps = 0;
	if (!cur || !best || !fs || !us) {
		ret = REG_ESPACE;
		goto out;
	}
	int prev0 = -1;
	for (size_t start = sj->start; start <= sj->end;) {
		int len0 = 1;
		int c0 = start < sj->end ? char_at(sj, start, &len0) : -1;
		for (size_t k = 0; k < nslots; k++) cur[k] = -1;
		regoff_t best_eo = -1;
		size_t nf = 0, nu = 0;
		fs[nf++] = (struct btframe){ 0, start, prev0, 0 };
		while (nf) {
			struct btframe f = fs[--nf];
			while (nu > f.undo) { nu--; cur[us[nu].slot] = us[nu].val; }
			int pc = f.pc;
			size_t i = f.i;
			int prev = f.prev;
			for (;;) {
				if (++steps > BT_BUDGET) {
					ret = REG_ESPACE;
					goto out;
				}
				const struct inst *in = &p->code[pc];
				int len, c;
				switch (in->op) {
				case I_CHAR: case I_ANY: case I_ANYNL: case I_SET:
					if (i >= sj->end) goto fail;
					c = char_at(sj, i, &len);
					if (!char_ok(p, in, c)) goto fail;
					prev = c;
					i += (size_t)len;
					pc++;
					continue;
				case I_BOL: case I_EOL: case I_WORDB: case I_NWORDB: case I_WBEG: case I_WEND:
					if (!assert_ok(sj, in->op, i, prev)) goto fail;
					pc++;
					continue;
				case I_JMP:
					pc = in->x;
					continue;
				case I_SPLIT:
					if (nf == fcap) {
						struct btframe *nfs = realloc(fs, 2 * fcap * sizeof *fs);
						if (!nfs) { ret = REG_ESPACE; goto out; }
						fs = nfs;
						fcap *= 2;
					}
					fs[nf++] = (struct btframe){ in->y, i, prev, nu };
					pc = in->x;
					continue;
				case I_SAVE:
				case I_MARK: {
					int slot = in->op == I_SAVE ? in->x : (int)ncap + in->x;
					if (nu == ucap) {
						struct undo *nus = realloc(us, 2 * ucap * sizeof *us);
						if (!nus) { ret = REG_ESPACE; goto out; }
						us = nus;
						ucap *= 2;
					}
					us[nu++] = (struct undo){ slot, cur[slot] };
					cur[slot] = (regoff_t)i;
					pc++;
					continue;
				}
				case I_CHKPROG:
					if (cur[ncap + (size_t)in->x] == (regoff_t)i) goto fail;   /* empty iteration */
					pc++;
					continue;
				case I_BREF: {
					regoff_t so = cur[2 * in->x], eo = cur[2 * in->x + 1];
					if (so < 0 || eo < 0) goto fail;
					size_t l = (size_t)(eo - so);
					if (i + l > sj->end) goto fail;
					if (p->icase) {
						for (size_t k = 0; k < l; k++)
							if (tolower(sj->s[so + (regoff_t)k]) != tolower(sj->s[i + k])) goto fail;
					} else if (memcmp(sj->s + so, sj->s + i, l)) {
						goto fail;
					}
					if (l) {
						int ln;
						size_t back = i + l - 1;
						if (sj->utf8)
							while (back > i && (sj->s[back] & 0xc0) == 0x80) back--;
						prev = char_at(sj, back, &ln);
					}
					i += l;
					pc++;
					continue;
				}
				case I_MATCH:
					if ((regoff_t)i > best_eo) {
						best_eo = (regoff_t)i;
						memcpy(best, cur, ncap * sizeof *best);
						best[0] = (regoff_t)start;
						best[1] = (regoff_t)i;
					}
					goto fail;
				}
			fail:
				break;
			}
		}
		if (best_eo >= 0) {
			memcpy(out, best, (ncap_out < ncap ? ncap_out : ncap) * sizeof *out);
			ret = 0;
			goto out;
		}
		if (p->anchored || start >= sj->end) break;
		prev0 = c0;
		start += (size_t)len0;
	}
out:
	free(cur);
	free(best);
	free(fs);
	free(us);
	return ret;
}

int regexec(const regex_t *restrict re, const char *restrict str, size_t nmatch,
            regmatch_t *restrict pm, int eflags)
{
	const struct prog *p = re->__prog;
	if (!p) return REG_BADPAT;
	if (re->__cflags & REG_NOSUB) nmatch = 0;
	struct subject sj;
	sj.s = (const unsigned char *)str;
	if (eflags & REG_STARTEND) {
		sj.start = (size_t)pm[0].rm_so;
		sj.end = (size_t)pm[0].rm_eo;
	} else {
		sj.start = 0;
		sj.end = strlen(str);
	}
	sj.utf8 = p->utf8;
	sj.notbol = !!(eflags & REG_NOTBOL);
	sj.noteol = !!(eflags & REG_NOTEOL);
	sj.icase = p->icase;
	sj.newline = p->newline;

	size_t ncap = 2 * (p->nsub + 1);
	size_t want = nmatch ? 2 * (nmatch < p->nsub + 1 ? nmatch : p->nsub + 1) : 2;
	regoff_t caps_small[20], *caps = ncap <= 20 ? caps_small : malloc(ncap * sizeof *caps);
	if (!caps) return REG_ESPACE;
	for (size_t k = 0; k < ncap; k++) caps[k] = -1;
	int r = p->has_bref ? backtrack(p, &sj, ncap, caps) : pike(p, &sj, want, caps);
	if (!r) {
		for (size_t k = 0; k < nmatch; k++) {
			if (2 * k + 1 < want && caps[2 * k] >= 0 && caps[2 * k + 1] >= 0) {
				pm[k].rm_so = caps[2 * k];
				pm[k].rm_eo = caps[2 * k + 1];
			} else {
				pm[k].rm_so = pm[k].rm_eo = -1;
			}
		}
	}
	if (caps != caps_small) free(caps);
	return r;
}

static const char *const messages[] = {
	[REG_OK] = "Success",
	[REG_NOMATCH] = "No match",
	[REG_BADPAT] = "Invalid regular expression",
	[REG_ECOLLATE] = "Invalid collation character",
	[REG_ECTYPE] = "Invalid character class name",
	[REG_EESCAPE] = "Trailing backslash",
	[REG_ESUBREG] = "Invalid back reference",
	[REG_EBRACK] = "Unmatched [ or [^",
	[REG_EPAREN] = "Unmatched ( or \\(",
	[REG_EBRACE] = "Unmatched \\{",
	[REG_BADBR] = "Invalid content of \\{\\}",
	[REG_ERANGE] = "Invalid range end",
	[REG_ESPACE] = "Memory exhausted",
	[REG_BADRPT] = "Invalid preceding regular expression",
};

size_t regerror(int e, const regex_t *restrict re, char *restrict buf, size_t size)
{
	(void)re;
	const char *m = e >= 0 && (size_t)e < ARRAY_SIZE(messages) && messages[e] ? messages[e] : "Unknown error";
	size_t l = strlen(m) + 1;
	if (size) {
		size_t n = l < size ? l : size;
		memcpy(buf, m, n - 1);
		buf[n - 1] = 0;
	}
	return l;
}
