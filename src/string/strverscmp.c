/*
 * lib-spfxd — strverscmp (GNU): compare strings treating digit runs as
 * numbers.  Runs with leading zeros are treated as fractional parts
 * ("0.01" < "0.1" style), runs without as integers (longer run wins).
 */
#include <string.h>
#include <ctype.h>

static __inline int dig(unsigned char c) { return c - '0' < 10u; }

int strverscmp(const char *l0, const char *r0)
{
	const unsigned char *l = (const void *)l0, *r = (const void *)r0;
	size_t i, dp = 0, z = 1;

	/* Find the first difference; remember where the digit run containing
	 * it starts and whether that run consisted only of zeros so far. */
	for (i = 0; l[i] == r[i]; i++) {
		unsigned char c = l[i];
		if (!c) return 0;
		if (!dig(c)) { dp = i + 1; z = 1; }
		else if (c != '0') z = 0;
	}

	if (l[dp] != '0' && r[dp] != '0') {
		/* Integer comparison: the longer digit run is larger. */
		size_t j;
		for (j = i; dig(l[j]); j++)
			if (!dig(r[j])) return 1;
		if (dig(r[j])) return -1;
	} else if (z && dp < i && (dig(l[i]) || dig(r[i]))) {
		/* Fractional runs ("00..."): a digit sorts before a non-digit. */
		return (unsigned char)(l[i] - '0') - (unsigned char)(r[i] - '0');
	}
	return l[i] - r[i];
}
