/* lib-spfxd — <getopt.h> */
#ifndef _GETOPT_H
#define _GETOPT_H
#include <features.h>
__SPFXD_BEGIN_DECLS
int getopt(int, char *const[], const char *);
extern char *optarg;
extern int optind, opterr, optopt, optreset;
struct option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};
#define no_argument       0
#define required_argument 1
#define optional_argument 2
int getopt_long(int, char *const *, const char *, const struct option *, int *);
int getopt_long_only(int, char *const *, const char *, const struct option *, int *);
__SPFXD_END_DECLS
#endif
