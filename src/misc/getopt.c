/*
 * lib-spfxd — getopt (POSIX) and getopt_long / getopt_long_only (GNU).
 *
 * POSIX semantics by default (stop at the first non-option); with getopt_long
 * non-option arguments are permuted to the end unless the option string
 * starts with '+' or POSIXLY_CORRECT is set, and '-' in that position
 * returns non-options as arguments of option character 1.  A leading ':'
 * suppresses diagnostics and makes a missing argument return ':'.
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *optarg;
int optind = 1, opterr = 1, optopt, optreset;
static int optpos;          /* position inside a cluster like -abc */

static void report(const char *prog, const char *msg, const char *opt, size_t len)
{
	if (!opterr) return;
	FILE *f = stderr;
	flockfile(f);
	fputs(prog, f);
	fputs(msg, f);
	fwrite(opt, 1, len, f);
	fputc('\n', f);
	funlockfile(f);
}

int getopt(int argc, char *const argv[], const char *optstring)
{
	if (optreset || !optind) {
		optreset = 0;
		optpos = 0;
		optind = 1;
	}
	if (optind >= argc || !argv[optind]) return -1;
	char *arg = argv[optind];
	if (!optpos) {
		if (arg[0] != '-' || !arg[1]) return -1;
		if (arg[1] == '-' && !arg[2]) {
			optind++;
			return -1;
		}
		optpos = 1;
	}

	const char *os = optstring;
	if (*os == '+' || *os == '-') os++;
	int colon = *os == ':';

	int c = (unsigned char)arg[optpos++];
	const char *spec = c != ':' ? strchr(os, c) : 0;
	if (!arg[optpos]) {
		optind++;
		optpos = 0;
	}
	if (!spec) {
		optopt = c;
		char ch = (char)c;
		if (!colon) report(argv[0], ": unrecognized option: ", &ch, 1);
		return '?';
	}
	if (spec[1] == ':') {
		if (spec[2] == ':') {
			/* optional argument: only if attached */
			optarg = optpos ? arg + optpos : 0;
			if (optpos) {
				optind++;
				optpos = 0;
			}
		} else if (optpos) {
			optarg = arg + optpos;
			optind++;
			optpos = 0;
		} else if (optind < argc) {
			optarg = argv[optind++];
		} else {
			optopt = c;
			char ch = (char)c;
			if (colon) return ':';
			report(argv[0], ": option requires an argument: ", &ch, 1);
			return '?';
		}
	}
	return c;
}

/* Move argv[from] to position `to`, shifting the elements between. */
static void permute(char *const *argv, int from, int to)
{
	char **av = (char **)argv;
	char *tmp = av[from];
	for (int i = from; i > to; i--) av[i] = av[i - 1];
	av[to] = tmp;
}

static int long_core(int argc, char *const *argv, const char *optstring,
	const struct option *longopts, int *idx, int only)
{
	optarg = 0;
	if (optreset || !optind) {
		optreset = 0;
		optpos = 0;
		optind = 1;
	}
	if (optind >= argc || !argv[optind]) return -1;

	int posixly = *optstring == '+' || getenv("POSIXLY_CORRECT");
	if (!optpos && (argv[optind][0] != '-' || !argv[optind][1])) {
		if (*optstring == '-') {
			optarg = argv[optind++];
			return 1;
		}
		if (posixly) return -1;
		/* find the next option and move it here */
		int i = optind;
		while (i < argc && argv[i] && (argv[i][0] != '-' || !argv[i][1])) i++;
		if (i >= argc || !argv[i]) return -1;
		int start = optind;
		optind = i;
		int cnt = 1;
		if (argv[i][0] == '-' && argv[i][1] == '-' && !argv[i][2]) {
			permute(argv, i, start);
			optind = start + 1;
			return -1;
		}
		int r = long_core(argc, argv, optstring, longopts, idx, only);
		/* move the option (and a separate argument, if consumed) before
		 * the skipped non-options */
		cnt = optind - i;
		for (int k = 0; k < cnt; k++) permute(argv, i + k, start + k);
		optind = start + cnt;
		return r;
	}

	char *arg = argv[optind];
	if (!optpos && arg[0] == '-' && ((arg[1] == '-' && arg[2]) || (only && arg[1] != '-'))) {
		const char *name = arg + (arg[1] == '-' ? 2 : 1);
		const char *eq = strchrnul(name, '=');
		size_t nl = (size_t)(eq - name);
		int match = -1, ambiguous = 0;
		for (int i = 0; longopts[i].name; i++) {
			if (strncmp(longopts[i].name, name, nl)) continue;
			if (strlen(longopts[i].name) == nl) {
				match = i;
				ambiguous = 0;
				break;
			}
			if (match >= 0) ambiguous = 1;
			else match = i;
		}
		if (match >= 0 && !ambiguous) {
			const struct option *o = &longopts[match];
			optind++;
			if (*eq) {
				if (o->has_arg == no_argument) {
					optopt = o->val;
					report(argv[0], ": option does not take an argument: ", name, nl);
					return '?';
				}
				optarg = (char *)eq + 1;
			} else if (o->has_arg == required_argument) {
				if (optind >= argc) {
					optopt = o->val;
					if (*optstring == ':' || (optstring[1] == ':' && (*optstring == '+' || *optstring == '-')))
						return ':';
					report(argv[0], ": option requires an argument: ", name, nl);
					return '?';
				}
				optarg = argv[optind++];
			}
			if (idx) *idx = match;
			if (o->flag) {
				*o->flag = o->val;
				return 0;
			}
			return o->val;
		}
		if (!only || arg[1] == '-' || !strchr(optstring, arg[1])) {
			optind++;
			optopt = 0;
			report(argv[0], ambiguous ? ": option is ambiguous: " : ": unrecognized option: ", name, nl);
			return '?';
		}
	}
	return getopt(argc, argv, optstring);
}

int getopt_long(int argc, char *const *argv, const char *optstring, const struct option *longopts, int *idx)
{
	return long_core(argc, argv, optstring, longopts, idx, 0);
}

int getopt_long_only(int argc, char *const *argv, const char *optstring, const struct option *longopts, int *idx)
{
	return long_core(argc, argv, optstring, longopts, idx, 1);
}
