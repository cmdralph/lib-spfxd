/*
 * lib-spfxd — time zones.
 *
 * Sources, in order of precedence:
 *   TZ unset        /etc/localtime
 *   TZ=""           UTC
 *   TZ=":name"      a TZif file (absolute, or below $TZDIR / /usr/share/zoneinfo)
 *   TZ="EST5EDT,…"  a POSIX TZ rule string, if it parses completely
 *   TZ="Area/City"  a TZif file
 * Unknown zones fall back to UTC.
 *
 * TZif files (RFC 8536, versions 1-4) are read whole; transitions are
 * binary-searched in place (big-endian, 64-bit in v2+ data).  Times after
 * the last transition use the file's footer rule.  Leap-second records
 * ("right/" zones) are honored: such zones count leap seconds in time_t,
 * and the inserted second is reported as tm_sec == 60.
 *
 * Process-wide state is protected by a lock; every query re-checks the TZ
 * variable so changes take effect without an explicit tzset().
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "time_impl.h"
#include "lock.h"

char *tzname[2] = { (char *)"UTC", (char *)"UTC" };
long timezone = 0;
int daylight = 0;
hidden const char *__tz_utc_name = "UTC";

struct when {
	char kind;         /* 'J' Julian 1-365, 'D' zero-based day, 'M' month.week.day */
	int m, w, d;
	long time;         /* seconds after local midnight */
};

struct rule {
	char std[TZNAME_MAX + 2], dst[TZNAME_MAX + 2];
	long std_off, dst_off;       /* seconds east of UTC */
	int has_dst;
	struct when start, end;
};

struct zone {
	unsigned char *raw;
	const unsigned char *trans, *idx, *types, *leaps;
	const char *abbrs;
	uint32_t timecnt, typecnt, charcnt, leapcnt;
	int tsize;                   /* 4 or 8 */
	struct rule rule;
	int has_rule;
};

static struct zone zone;
static char *cur_tz;            /* TZ value the zone was built from */
static int loaded;
static volatile int tz_lock;

static uint32_t be32(const unsigned char *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static int64_t be_time(const unsigned char *p, int size)
{
	if (size == 4) return (int32_t)be32(p);
	return (int64_t)((uint64_t)be32(p) << 32 | be32(p + 4));
}

/* ---------------------------------------------------------------------- */
/* POSIX TZ strings                                                        */
/* ---------------------------------------------------------------------- */

static int parse_name(const char **ps, char *out)
{
	const char *s = *ps;
	size_t n = 0;
	if (*s == '<') {
		s++;
		while (*s && *s != '>') {
			if (n < TZNAME_MAX + 1) out[n++] = *s;
			s++;
		}
		if (*s != '>') return -1;
		s++;
	} else {
		while ((*s | 32) - 'a' < 26u) {
			if (n < TZNAME_MAX + 1) out[n++] = *s;
			s++;
		}
	}
	if (n < 3) return -1;
	out[n] = 0;
	*ps = s;
	return 0;
}

/* [+-]hh[:mm[:ss]] -> seconds (sign as written) */
static int parse_hms(const char **ps, long *out, long maxh)
{
	const char *s = *ps;
	int neg = 0;
	long h = 0, m = 0, sec = 0;
	if (*s == '+' || *s == '-') neg = *s++ == '-';
	if (*s - '0' >= 10u) return -1;
	while (*s - '0' < 10u) h = h * 10 + (*s++ - '0');
	if (h > maxh) return -1;
	if (*s == ':') {
		s++;
		if (*s - '0' >= 10u) return -1;
		while (*s - '0' < 10u) m = m * 10 + (*s++ - '0');
		if (*s == ':') {
			s++;
			if (*s - '0' >= 10u) return -1;
			while (*s - '0' < 10u) sec = sec * 10 + (*s++ - '0');
		}
	}
	if (m > 59 || sec > 59) return -1;
	*out = (neg ? -1 : 1) * (h * 3600 + m * 60 + sec);
	*ps = s;
	return 0;
}

static int parse_num(const char **ps, int lo, int hi, int *out)
{
	const char *s = *ps;
	int v = 0;
	if (*s - '0' >= 10u) return -1;
	while (*s - '0' < 10u) {
		v = v * 10 + (*s++ - '0');
		if (v > hi) return -1;
	}
	if (v < lo) return -1;
	*out = v;
	*ps = s;
	return 0;
}

static int parse_when(const char **ps, struct when *w)
{
	const char *s = *ps;
	if (*s == 'J') {
		s++;
		w->kind = 'J';
		if (parse_num(&s, 1, 365, &w->d)) return -1;
	} else if (*s == 'M') {
		s++;
		w->kind = 'M';
		if (parse_num(&s, 1, 12, &w->m) || *s++ != '.') return -1;
		if (parse_num(&s, 1, 5, &w->w) || *s++ != '.') return -1;
		if (parse_num(&s, 0, 6, &w->d)) return -1;
	} else {
		w->kind = 'D';
		if (parse_num(&s, 0, 365, &w->d)) return -1;
	}
	w->time = 7200;
	if (*s == '/') {
		s++;
		if (parse_hms(&s, &w->time, 167)) return -1;
	}
	*ps = s;
	return 0;
}

static int parse_posix(const char *s, struct rule *r)
{
	long off;
	memset(r, 0, sizeof *r);
	if (parse_name(&s, r->std) || parse_hms(&s, &off, 24)) return -1;
	r->std_off = -off;
	if (!*s) return 0;
	if (parse_name(&s, r->dst)) return -1;
	r->has_dst = 1;
	r->dst_off = r->std_off + 3600;
	if (*s && *s != ',') {
		if (parse_hms(&s, &off, 24)) return -1;
		r->dst_off = -off;
	}
	if (!*s) {
		/* no rule given: the long-standing US default */
		r->start = (struct when){ 'M', 3, 2, 0, 7200 };
		r->end = (struct when){ 'M', 11, 1, 0, 7200 };
		return 0;
	}
	if (*s++ != ',' || parse_when(&s, &r->start) || *s++ != ',' || parse_when(&s, &r->end))
		return -1;
	return *s ? -1 : 0;
}

static int is_leap(int64_t y)
{
	return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

/* Seconds since the epoch (UTC) at which rule date w occurs in year y,
 * given the offset in effect just before it. */
static int64_t rule_time(const struct when *w, int64_t y, long off_before)
{
	int64_t jan1 = __days_from_civil(y, 1, 1), day;
	switch (w->kind) {
	case 'J':
		day = w->d - 1 + (is_leap(y) && w->d >= 60);
		break;
	case 'D':
		day = w->d;
		break;
	default: {
		int64_t first = __days_from_civil(y, w->m, 1);
		int wday_first = (int)(((first % 7) + 11) % 7);
		int delta = (w->d - wday_first + 7) % 7;
		int64_t dd = first + delta + 7 * (w->w - 1);
		static const int mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		int ml = mdays[w->m - 1] + (w->m == 2 && is_leap(y));
		while (dd >= first + ml) dd -= 7;
		day = dd - jan1;
		break;
	}
	}
	return (jan1 + day) * 86400 + w->time - off_before;
}

static int rule_is_dst(const struct rule *r, int64_t t)
{
	if (!r->has_dst) return 0;
	struct tm tm;
	if (__secs_to_tm(t + r->std_off, &tm)) return 0;
	int64_t y = (int64_t)tm.tm_year + 1900;
	/* check this year and the neighbours so transitions near New Year,
	 * or with out-of-range times, are attributed correctly */
	for (int64_t yy = y - 1; yy <= y + 1; yy++) {
		int64_t s = rule_time(&r->start, yy, r->std_off);
		int64_t e = rule_time(&r->end, yy, r->dst_off);
		if (s < e) {
			if (t >= s && t < e) return 1;
		} else {
			/* southern hemisphere: DST spans New Year */
			int64_t e_next = rule_time(&r->end, yy + 1, r->dst_off);
			if (t >= s && t < e_next) return 1;
		}
	}
	return 0;
}

/* ---------------------------------------------------------------------- */
/* TZif files                                                              */
/* ---------------------------------------------------------------------- */

static unsigned char *read_file(const char *path, size_t *len)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) return 0;
	struct stat st;
	unsigned char *buf = 0;
	if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 44 || st.st_size > (1 << 20)) goto out;
	buf = malloc((size_t)st.st_size + 1);
	if (!buf) goto out;
	size_t got = 0;
	while (got < (size_t)st.st_size) {
		ssize_t r = read(fd, buf + got, (size_t)st.st_size - got);
		if (r <= 0) {
			free(buf);
			buf = 0;
			goto out;
		}
		got += (size_t)r;
	}
	buf[got] = 0;
	*len = got;
out:
	close(fd);
	return buf;
}

static int parse_tzif(unsigned char *raw, size_t len, struct zone *z)
{
	if (memcmp(raw, "TZif", 4)) return -1;
	int version = raw[4] ? raw[4] - '0' : 1;
	const unsigned char *p = raw;
	size_t off = 0;
	int tsize = 4;
	for (int pass = 0; pass < 2; pass++) {
		if (off + 44 > len) return -1;
		const unsigned char *h = p + off;
		uint32_t isut = be32(h + 20), isstd = be32(h + 24), leap = be32(h + 28);
		uint32_t timecnt = be32(h + 32), typecnt = be32(h + 36), charcnt = be32(h + 40);
		if (!typecnt || typecnt > 256 || timecnt > 100000 || leap > 1000) return -1;
		size_t body = (size_t)timecnt * (tsize + 1) + (size_t)typecnt * 6 + charcnt +
			(size_t)leap * (tsize + 4) + isstd + isut;
		if (off + 44 + body > len) return -1;
		if (pass == 0 && version >= 2) {
			off += 44 + body;
			tsize = 8;
			continue;
		}
		const unsigned char *d = h + 44;
		z->tsize = tsize;
		z->timecnt = timecnt;
		z->trans = d;
		d += (size_t)timecnt * tsize;
		z->idx = d;
		d += timecnt;
		z->types = d;
		d += (size_t)typecnt * 6;
		z->abbrs = (const char *)d;
		d += charcnt;
		z->leaps = d;
		z->typecnt = typecnt;
		z->charcnt = charcnt;
		z->leapcnt = leap;
		for (uint32_t i = 0; i < timecnt; i++)
			if (z->idx[i] >= typecnt) return -1;
		for (uint32_t i = 0; i < typecnt; i++)
			if (z->types[i * 6 + 5] >= charcnt) return -1;
		if (charcnt == 0 || z->abbrs[charcnt - 1]) return -1;
		off += 44 + body;
		/* footer: "\n<rule>\n" */
		if (tsize == 8 && off < len && p[off] == '\n') {
			const char *rs = (const char *)p + off + 1;
			char *nl = memchr(rs, '\n', len - off - 1);
			if (nl) {
				*nl = 0;
				if (*rs && !parse_posix(rs, &z->rule)) z->has_rule = 1;
			}
		}
		return 0;
	}
	return -1;
}

static int load_file(const char *name, struct zone *z)
{
	char path[PATH_MAX];
	if (*name == ':') name++;
	if (*name == '/') {
		if (__libc.secure && strncmp(name, "/usr/share/zoneinfo/", 20) && strcmp(name, "/etc/localtime"))
			return -1;
		if (strlen(name) >= sizeof path) return -1;
		strcpy(path, name);
	} else {
		/* relative names may not escape the zone directory */
		for (const char *c = name; *c; c++)
			if (c[0] == '.' && c[1] == '.' && (c == name || c[-1] == '/')) return -1;
		const char *dir = __libc.secure ? 0 : getenv("TZDIR");
		if (!dir || !*dir) dir = "/usr/share/zoneinfo";
		size_t dl = strlen(dir), nl = strlen(name);
		if (dl + nl + 2 > sizeof path) return -1;
		memcpy(path, dir, dl);
		path[dl] = '/';
		memcpy(path + dl + 1, name, nl + 1);
	}
	size_t len;
	unsigned char *raw = read_file(path, &len);
	if (!raw) return -1;
	memset(z, 0, sizeof *z);
	if (parse_tzif(raw, len, z)) {
		free(raw);
		return -1;
	}
	z->raw = raw;
	return 0;
}

/* ---------------------------------------------------------------------- */
/* zone selection                                                          */
/* ---------------------------------------------------------------------- */

static void set_utc(struct zone *z)
{
	memset(z, 0, sizeof *z);
	strcpy(z->rule.std, "UTC");
	z->has_rule = 1;
}

static void publish_globals(void)
{
	static char stdname[TZNAME_MAX + 2], dstname[TZNAME_MAX + 2];
	long std_off = 0, dst_off = 0;
	int has_dst = 0;
	const char *sn = "UTC", *dn = 0;
	if (zone.has_rule) {
		sn = zone.rule.std;
		std_off = zone.rule.std_off;
		if (zone.rule.has_dst) {
			has_dst = 1;
			dn = zone.rule.dst;
			dst_off = zone.rule.dst_off;
		}
	}
	if (zone.typecnt) {
		/* the most recent standard and daylight types in the file */
		int found_std = zone.has_rule, found_dst = zone.has_rule && has_dst;
		for (uint32_t i = zone.timecnt; i-- > 0 && (!found_std || !found_dst); ) {
			const unsigned char *ty = zone.types + 6 * zone.idx[i];
			if (ty[4] && !found_dst) {
				dn = zone.abbrs + ty[5];
				dst_off = (int32_t)be32(ty);
				found_dst = has_dst = 1;
			} else if (!ty[4] && !found_std) {
				sn = zone.abbrs + ty[5];
				std_off = (int32_t)be32(ty);
				found_std = 1;
			}
		}
		if (!found_std) {
			sn = zone.abbrs + zone.types[5];
			std_off = (int32_t)be32(zone.types);
		}
	}
	strncpy(stdname, sn, sizeof stdname - 1);
	strncpy(dstname, dn ? dn : sn, sizeof dstname - 1);
	tzname[0] = stdname;
	tzname[1] = dstname;
	timezone = -std_off;
	daylight = has_dst;
	(void)dst_off;
}

/* Called with tz_lock held. */
static void tz_update(void)
{
	const char *tz = getenv("TZ");
	if (loaded && ((!tz && !cur_tz) || (tz && cur_tz && !strcmp(tz, cur_tz)))) return;

	free(zone.raw);
	memset(&zone, 0, sizeof zone);
	free(cur_tz);
	cur_tz = tz ? strdup(tz) : 0;
	loaded = 1;

	if (!tz) {
		if (load_file("/etc/localtime", &zone)) set_utc(&zone);
	} else if (!*tz) {
		set_utc(&zone);
	} else if (*tz == ':') {
		if (load_file(tz, &zone)) set_utc(&zone);
	} else if (!strchr(tz, '/') && !parse_posix(tz, &zone.rule)) {
		zone.has_rule = 1;
	} else if (load_file(tz, &zone)) {
		set_utc(&zone);
	}
	publish_globals();
}

void tzset(void)
{
	__lock_always(&tz_lock);
	tz_update();
	__unlock_always(&tz_lock);
}

/* Offset, DST flag and abbreviation for a time t (in the zone's time_t,
 * i.e. including leap seconds for "right/" zones). */
static void lookup(int64_t t, long *off, int *dst, const char **abbr)
{
	if (zone.timecnt) {
		int64_t first = be_time(zone.trans, zone.tsize);
		int64_t last = be_time(zone.trans + (size_t)(zone.timecnt - 1) * zone.tsize, zone.tsize);
		if (t >= first && !(t >= last && zone.has_rule)) {
			uint32_t lo = 0, hi = zone.timecnt;
			while (hi - lo > 1) {
				uint32_t mid = lo + (hi - lo) / 2;
				if (be_time(zone.trans + (size_t)mid * zone.tsize, zone.tsize) <= t) lo = mid;
				else hi = mid;
			}
			const unsigned char *ty = zone.types + 6 * zone.idx[lo];
			*off = (int32_t)be32(ty);
			*dst = ty[4];
			*abbr = zone.abbrs + ty[5];
			return;
		}
	}
	if (zone.has_rule) {
		int d = rule_is_dst(&zone.rule, t);
		*off = d ? zone.rule.dst_off : zone.rule.std_off;
		*dst = d;
		*abbr = d ? zone.rule.dst : zone.rule.std;
		return;
	}
	/* before the first transition: time type 0 (RFC 8536) */
	const unsigned char *ty = zone.types;
	*off = (int32_t)be32(ty);
	*dst = ty[4];
	*abbr = zone.abbrs + ty[5];
}

/* Leap-second correction in effect at t; *hit set when t is itself an
 * inserted leap second. */
static long leap_corr(int64_t t, int *hit)
{
	long corr = 0, prev = 0;
	*hit = 0;
	size_t rec = (size_t)zone.tsize + 4;
	for (uint32_t i = 0; i < zone.leapcnt; i++) {
		const unsigned char *l = zone.leaps + i * rec;
		int64_t occ = be_time(l, zone.tsize);
		if (occ > t) break;
		prev = corr;
		corr = (int32_t)be32(l + zone.tsize);
		*hit = occ == t && corr > prev;
	}
	return corr;
}

hidden void __tz_local(int64_t t, long *gmtoff, int *isdst, const char **abbr,
	long *leapcorr, int *leap_sec)
{
	__lock_always(&tz_lock);
	tz_update();
	int hit = 0;
	*leapcorr = zone.leapcnt ? leap_corr(t, &hit) : 0;
	*leap_sec = hit;
	lookup(t, gmtoff, isdst, abbr);
	__unlock_always(&tz_lock);
}

/*
 * mktime core: find t such that local(t) == local_secs (the broken-down
 * local time expressed as if it were UTC).  Candidate offsets come from the
 * zone one day either side, which brackets any transition near the time.
 * A time that occurs twice (DST ending) is resolved by tm_isdst when it is
 * not negative (the earlier instant otherwise); a time that does not exist
 * (DST starting) is moved forward across the gap.  A tm_isdst that
 * contradicts the zone is honored by interpreting the time in the
 * requested kind of time (standard or daylight).
 */
hidden int64_t __tz_mktime(struct tm *tm, int64_t local_secs)
{
	long offs[2], o;
	int dsts[2], d, hit;
	const char *ab;
	int64_t best = 0;
	int found = 0;

	__lock_always(&tz_lock);
	tz_update();
	lookup(local_secs - 86400, &offs[0], &dsts[0], &ab);
	lookup(local_secs + 86400, &offs[1], &dsts[1], &ab);

	for (int i = 0; i < 2 && !found; i++) {
		int64_t t = local_secs - offs[i];
		lookup(t, &o, &d, &ab);
		if (o != offs[i]) continue;
		if (tm->tm_isdst >= 0 && d != (tm->tm_isdst > 0)) {
			/* consistent instant, but of the other kind: try the
			 * other candidate first (overlap) */
			if (!found) best = t;
			found = 2;
			continue;
		}
		best = t;
		found = 1;
	}
	if (found == 2 && tm->tm_isdst >= 0) {
		/* reinterpret in the requested kind of time */
		long std = -timezone;
		long want = tm->tm_isdst > 0 ? (zone.has_rule && zone.rule.has_dst ? zone.rule.dst_off : std + 3600) : std;
		lookup(best, &o, &d, &ab);
		best = local_secs - want;
	} else if (!found) {
		/* nonexistent local time: shift forward across the gap */
		long before = offs[0];
		if (tm->tm_isdst > 0 && dsts[1]) before = offs[0];
		best = local_secs - before;
	}
	long corr = zone.leapcnt ? leap_corr(best, &hit) : 0;
	best += corr;
	__unlock_always(&tz_lock);
	return best;
}
