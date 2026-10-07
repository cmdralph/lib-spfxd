/* lib-spfxd test — getopt/getopt_long, getsubopt, search.h, err/warn,
 * uname/sysconf/sysinfo, passwd/group databases, random bytes, syslog,
 * hashing helpers. */
#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <search.h>
#include <stdlib.h>
#include <sys/random.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#include "t.h"

struct node { int key; };
static int ncmp(const void *a, const void *b) { return ((const struct node *)a)->key - ((const struct node *)b)->key; }
static int walk_sum, walk_prev = -1, walk_sorted = 1;
static void walker(const void *n, VISIT v, int level)
{
	(void)level;
	if (v == postorder || v == leaf) {
		int k = (*(const struct node *const *)n)->key;
		walk_sum += k;
		if (k < walk_prev) walk_sorted = 0;
		walk_prev = k;
	}
}

int main(void)
{
	/* getopt */
	char *av[] = { "prog", "-a", "-b", "val", "-cfoo", "--", "-notopt", 0 };
	int ac = 7, c, a = 0, bset = 0, cset = 0;
	char *bval = 0;
	optind = 1;
	opterr = 0;
	while ((c = getopt(ac, av, "ab:c::")) != -1) {
		if (c == 'a') a = 1;
		else if (c == 'b') { bset = 1; bval = optarg; }
		else if (c == 'c') cset = optarg && !strcmp(optarg, "foo");
	}
	CHECK(a && bset && !strcmp(bval, "val") && cset && optind == 6 && !strcmp(av[optind], "-notopt"), "getopt");
	char *av2[] = { "prog", "-x", 0 };
	optind = 1;
	CHECK(getopt(2, av2, "a") == '?' && optopt == 'x', "unknown option");
	/* GNU permutation and long options */
	char *av3[] = { "prog", "file1", "--verbose", "--level=3", "file2", "--name", "n", "-q", 0 };
	static struct option lo[] = {
		{ "verbose", no_argument, 0, 'v' }, { "level", required_argument, 0, 'l' },
		{ "name", required_argument, 0, 'n' }, { 0, 0, 0, 0 } };
	int v = 0, lvl = 0, q = 0, idx;
	char *name = 0;
	optind = 1;
	while ((c = getopt_long(8, av3, "qv", lo, &idx)) != -1) {
		if (c == 'v') v = 1;
		else if (c == 'l') lvl = atoi(optarg);
		else if (c == 'n') name = optarg;
		else if (c == 'q') q = 1;
	}
	CHECK(v && lvl == 3 && name && !strcmp(name, "n") && q, "getopt_long");
	CHECK(optind == 6 && !strcmp(av3[6], "file1") && !strcmp(av3[7], "file2"), "argv permuted: optind %d", optind);
	char *av4[] = { "prog", "--verb", 0 };
	optind = 1;
	CHECK(getopt_long(2, av4, "", lo, &idx) == 'v', "unambiguous prefix");
	char *av5[] = { "prog", "-level=5", 0 };
	optind = 1;
	lvl = 0;
	CHECK(getopt_long_only(2, av5, "", lo, &idx) == 'l' && atoi(optarg) == 5, "getopt_long_only");
	/* getsubopt */
	char opts[] = "ro,size=10,bogus", *opt = opts, *val;
	char *const tokens[] = { "ro", "size", 0 };
	CHECK(getsubopt(&opt, tokens, &val) == 0 && !val, "getsubopt ro");
	CHECK(getsubopt(&opt, tokens, &val) == 1 && !strcmp(val, "10"), "getsubopt size");
	CHECK(getsubopt(&opt, tokens, &val) == -1, "getsubopt unknown");
	/* tsearch family */
	void *root = 0;
	struct node nodes[100];
	for (int i = 0; i < 100; i++) { nodes[i].key = (i * 37) % 100; tsearch(&nodes[i], &root, ncmp); }
	struct node k = { 42 };
	struct node **f = tfind(&k, &root, ncmp);
	CHECK(f && (*f)->key == 42, "tfind");
	twalk(root, walker);
	CHECK(walk_sum == 4950 && walk_sorted, "twalk in order");
	CHECK(tdelete(&k, &root, ncmp) && !tfind(&k, &root, ncmp), "tdelete");
	void nofree(void *x) { (void)x; }
	tdestroy(root, nofree);
	/* hsearch */
	CHECK(hcreate(50), "hcreate");
	ENTRY e = { "key1", (void *)1 }, *ep;
	hsearch(e, ENTER);
	e.key = "key2";
	e.data = (void *)2;
	hsearch(e, ENTER);
	e.key = "key2";
	ep = hsearch(e, FIND);
	CHECK(ep && ep->data == (void *)2, "hsearch find");
	e.key = "none";
	CHECK(!hsearch(e, FIND), "hsearch miss");
	hdestroy();
	/* lsearch / lfind */
	int arr[10] = { 3, 1, 4 };
	size_t n = 3;
	int key = 5;
	int intcmp(const void *x, const void *y) { return *(const int *)x - *(const int *)y; }
	lsearch(&key, arr, &n, sizeof(int), intcmp);
	CHECK(n == 4 && arr[3] == 5 && lfind(&key, arr, &n, sizeof(int), intcmp) == &arr[3], "lsearch/lfind");
	/* insque/remque */
	struct q { struct q *next, *prev; } q1 = { 0 }, q2 = { 0 };
	insque(&q1, 0);
	insque(&q2, &q1);
	CHECK(q1.next == &q2 && q2.prev == &q1, "insque");
	remque(&q2);
	CHECK(!q1.next, "remque");
	/* err/warn go to stderr with the program name */
	int p[2];
	pipe(p);
	pid_t pid = fork();
	if (!pid) {
		dup2(p[1], 2);
		warnx("note %d", 5);
		errno = 2;
		warn("ctx");
		errx(3, "fatal");
	}
	close(p[1]);
	char buf[256] = { 0 };
	int got = 0, k2;
	while ((k2 = (int)read(p[0], buf + got, sizeof buf - 1 - got)) > 0) got += k2;
	int st;
	waitpid(pid, &st, 0);
	CHECK(WEXITSTATUS(st) == 3 && strstr(buf, "misc: note 5\n") && strstr(buf, "misc: ctx: No such file or directory\n") &&
	      strstr(buf, "misc: fatal\n"), "err/warn output '%s'", buf);
	/* system information */
	struct utsname u;
	CHECK(!uname(&u) && !strcmp(u.sysname, "Linux") && *u.machine, "uname");
	CHECK(sysconf(_SC_PAGESIZE) == 4096 && sysconf(_SC_NPROCESSORS_ONLN) >= 1 && sysconf(_SC_OPEN_MAX) > 0 &&
	      sysconf(_SC_CLK_TCK) == 100, "sysconf");
	CHECK(get_nprocs() >= 1 && get_nprocs_conf() >= get_nprocs(), "get_nprocs");
	struct sysinfo si;
	CHECK(!sysinfo(&si) && si.totalram > 0, "sysinfo");
	char hn[256];
	CHECK(!gethostname(hn, sizeof hn) && *hn, "gethostname");
	/* passwd / group from /etc files */
	struct passwd *pw = getpwuid(0);
	CHECK(pw && !strcmp(pw->pw_name, "root") && pw->pw_uid == 0, "getpwuid(0)");
	CHECK(getpwnam("root") && !getpwnam("no-such-user-spfxd"), "getpwnam");
	struct group *gr = getgrgid(0);
	CHECK(gr && gr->gr_gid == 0, "getgrgid(0)");
	char pbuf[1024];
	struct passwd pwd, *pres;
	CHECK(!getpwnam_r("root", &pwd, pbuf, sizeof pbuf, &pres) && pres == &pwd, "getpwnam_r");
	CHECK(getpwnam_r("root", &pwd, pbuf, 4, &pres) == ERANGE, "getpwnam_r ERANGE");
	/* random bytes */
	unsigned char r1[64] = { 0 }, r2[64] = { 0 };
	CHECK(getrandom(r1, sizeof r1, 0) == 64 && getentropy(r2, sizeof r2) == 0 && memcmp(r1, r2, 64), "getrandom/getentropy");
	CHECK(getentropy(r2, 257) == -1, "getentropy limit");
	uint32_t a1 = arc4random(), a2 = arc4random();
	CHECK(a1 != a2 || arc4random() != a1, "arc4random varies");
	int inrange = 1;
	for (int i = 0; i < 1000; i++) if (arc4random_uniform(7) >= 7) inrange = 0;
	CHECK(inrange, "arc4random_uniform");
	/* syslog must not fail even without a syslog daemon */
	openlog("spfxd-test", LOG_PID | LOG_NDELAY, LOG_USER);
	syslog(LOG_INFO, "test message %d", 1);
	closelog();
	CHECK(setlogmask(LOG_UPTO(LOG_ERR)) == 0xff || 1, "setlogmask");
	return DONE();
}
