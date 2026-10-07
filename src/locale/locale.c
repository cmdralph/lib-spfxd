/* lib-spfxd — setlocale, localeconv, newlocale/duplocale/freelocale/uselocale,
 * nl_langinfo.  See locale_impl.h for what is supported. */
#include <errno.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "locale_impl.h"
#include "pthread_impl.h"

hidden const struct __spfxd_locale __c_locale = {
	0, { "C", "C", "C", "C", "C", "C" }
};
hidden struct __spfxd_locale __global_locale = {
	0, { "C", "C", "C", "C", "C", "C" }
};

static volatile int locale_lock;

static const char *const cat_names[LC_ALL] = {
	"LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES"
};

hidden const struct __spfxd_locale *__current_locale(void)
{
	struct __spfxd_locale *l = __self()->locale;
	return l ? l : &__global_locale;
}

size_t __spfxd_mb_cur_max(void)
{
	return __locale_utf8() ? 4 : 1;
}

/* Is name a locale we can provide?  *utf8 reports its LC_CTYPE encoding. */
static int valid_name(const char *name, int *utf8)
{
	size_t l = strlen(name);
	if (!l || l >= LOCALE_NAME_MAX || strchr(name, '/')) return 0;
	if (!strcmp(name, "C") || !strcmp(name, "POSIX")) {
		*utf8 = 0;
		return 1;
	}
	const char *dot = strchr(name, '.');
	if (dot) {
		const char *cs = dot + 1;
		size_t cl = strcspn(cs, "@");
		if ((cl == 5 && !strncasecmp(cs, "utf-8", 5)) || (cl == 4 && !strncasecmp(cs, "utf8", 4))) {
			*utf8 = 1;
			return 1;
		}
	}
	return 0;
}

static const char *env_name(int cat)
{
	const char *s;
	if ((s = getenv("LC_ALL")) && *s) return s;
	if ((s = getenv(cat_names[cat])) && *s) return s;
	if ((s = getenv("LANG")) && *s) return s;
	return "C";
}

/* Apply one category; returns 0 on success. */
static int set_cat(struct __spfxd_locale *loc, int cat, const char *name)
{
	int utf8;
	if (!*name) name = env_name(cat);
	if (!valid_name(name, &utf8)) return -1;
	if (cat == LC_CTYPE) loc->utf8 = utf8;
	strcpy(loc->names[cat], name);
	return 0;
}

static char *query_all(const struct __spfxd_locale *loc)
{
	static char buf[LC_ALL * (LOCALE_NAME_MAX + 16)];
	int same = 1;
	for (int i = 1; i < LC_ALL; i++)
		if (strcmp(loc->names[i], loc->names[0])) same = 0;
	if (same) return (char *)loc->names[0];
	size_t n = 0;
	for (int i = 0; i < LC_ALL; i++) {
		size_t a = strlen(cat_names[i]), b = strlen(loc->names[i]);
		memcpy(buf + n, cat_names[i], a);
		n += a;
		buf[n++] = '=';
		memcpy(buf + n, loc->names[i], b);
		n += b;
		buf[n++] = i + 1 < LC_ALL ? ';' : 0;
	}
	return buf;
}

/* Parse a composite "LC_CTYPE=x;LC_NUMERIC=y;..." string. */
static int set_composite(struct __spfxd_locale *loc, const char *s)
{
	struct __spfxd_locale tmp = *loc;
	char part[LOCALE_NAME_MAX];
	while (*s) {
		const char *eq = strchr(s, '='), *end = strchrnul(s, ';');
		if (!eq || eq > end) return -1;
		int cat = -1;
		for (int i = 0; i < LC_ALL; i++)
			if ((size_t)(eq - s) == strlen(cat_names[i]) && !strncmp(s, cat_names[i], (size_t)(eq - s)))
				cat = i;
		size_t vl = (size_t)(end - eq - 1);
		if (cat < 0 || !vl || vl >= sizeof part) return -1;
		memcpy(part, eq + 1, vl);
		part[vl] = 0;
		if (set_cat(&tmp, cat, part)) return -1;
		s = *end ? end + 1 : end;
	}
	*loc = tmp;
	return 0;
}

char *setlocale(int cat, const char *name)
{
	if ((unsigned)cat > LC_ALL) return 0;
	__lock_always(&locale_lock);
	char *r = 0;
	if (!name) {
		r = cat == LC_ALL ? query_all(&__global_locale) : __global_locale.names[cat];
	} else if (cat == LC_ALL) {
		struct __spfxd_locale tmp = __global_locale;
		int ok = 1;
		if (strchr(name, ';') || strchr(name, '=')) {
			ok = !set_composite(&tmp, name);
		} else {
			for (int i = 0; i < LC_ALL && ok; i++) ok = !set_cat(&tmp, i, name);
		}
		if (ok) {
			__global_locale = tmp;
			r = query_all(&__global_locale);
		}
	} else if (!set_cat(&__global_locale, cat, name)) {
		r = __global_locale.names[cat];
	}
	__unlock_always(&locale_lock);
	return r;
}

struct lconv *localeconv(void)
{
	static struct lconv c = {
		.decimal_point = (char *)".",
		.thousands_sep = (char *)"",
		.grouping = (char *)"",
		.int_curr_symbol = (char *)"",
		.currency_symbol = (char *)"",
		.mon_decimal_point = (char *)"",
		.mon_thousands_sep = (char *)"",
		.mon_grouping = (char *)"",
		.positive_sign = (char *)"",
		.negative_sign = (char *)"",
		.int_frac_digits = CHAR_MAX,
		.frac_digits = CHAR_MAX,
		.p_cs_precedes = CHAR_MAX,
		.p_sep_by_space = CHAR_MAX,
		.n_cs_precedes = CHAR_MAX,
		.n_sep_by_space = CHAR_MAX,
		.p_sign_posn = CHAR_MAX,
		.n_sign_posn = CHAR_MAX,
		.int_p_cs_precedes = CHAR_MAX,
		.int_p_sep_by_space = CHAR_MAX,
		.int_n_cs_precedes = CHAR_MAX,
		.int_n_sep_by_space = CHAR_MAX,
		.int_p_sign_posn = CHAR_MAX,
		.int_n_sign_posn = CHAR_MAX,
	};
	return &c;
}

locale_t newlocale(int mask, const char *name, locale_t base)
{
	struct __spfxd_locale tmp;
	if ((unsigned)mask & ~(unsigned)LC_ALL_MASK) {
		errno = EINVAL;
		return 0;
	}
	if (base && base != LC_GLOBAL_LOCALE) tmp = *base;
	else tmp = __c_locale;
	for (int i = 0; i < LC_ALL; i++) {
		if (!(mask & (1 << i))) continue;
		if (set_cat(&tmp, i, name)) {
			errno = ENOENT;
			return 0;
		}
	}
	if (base && base != LC_GLOBAL_LOCALE) {
		*base = tmp;
		return base;
	}
	struct __spfxd_locale *l = malloc(sizeof *l);
	if (!l) return 0;
	*l = tmp;
	return l;
}

locale_t duplocale(locale_t old)
{
	struct __spfxd_locale *l = malloc(sizeof *l);
	if (!l) return 0;
	*l = old == LC_GLOBAL_LOCALE ? __global_locale : *old;
	return l;
}

void freelocale(locale_t l)
{
	free(l);
}

locale_t uselocale(locale_t new)
{
	struct pthread *self = __self();
	locale_t old = self->locale ? self->locale : LC_GLOBAL_LOCALE;
	if (new) self->locale = new == LC_GLOBAL_LOCALE ? 0 : new;
	return old;
}

char *nl_langinfo_l(nl_item item, locale_t loc)
{
	static const char *const days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char *const ldays[] = { "Sunday", "Monday", "Tuesday", "Wednesday",
		"Thursday", "Friday", "Saturday" };
	static const char *const mons[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	static const char *const lmons[] = { "January", "February", "March", "April", "May",
		"June", "July", "August", "September", "October", "November", "December" };
	const struct __spfxd_locale *l = loc && loc != LC_GLOBAL_LOCALE ? loc : __current_locale();
	if (item >= ABDAY_1 && item <= ABDAY_7) return (char *)days[item - ABDAY_1];
	if (item >= DAY_1 && item <= DAY_7) return (char *)ldays[item - DAY_1];
	if (item >= ABMON_1 && item <= ABMON_12) return (char *)mons[item - ABMON_1];
	if (item >= MON_1 && item <= MON_12) return (char *)lmons[item - MON_1];
	switch (item) {
	case CODESET: return (char *)(l->utf8 ? "UTF-8" : "ANSI_X3.4-1968");
	case D_T_FMT: return (char *)"%a %b %e %H:%M:%S %Y";
	case D_FMT: return (char *)"%m/%d/%y";
	case T_FMT: return (char *)"%H:%M:%S";
	case T_FMT_AMPM: return (char *)"%I:%M:%S %p";
	case AM_STR: return (char *)"AM";
	case PM_STR: return (char *)"PM";
	case RADIXCHAR: return (char *)".";
	case THOUSEP: return (char *)"";
	case YESEXPR: return (char *)"^[yY]";
	case NOEXPR: return (char *)"^[nN]";
	case CRNCYSTR: return (char *)"-";
	}
	return (char *)"";
}

char *nl_langinfo(nl_item item)
{
	return nl_langinfo_l(item, 0);
}
