/* lib-spfxd — hcreate / hsearch / hdestroy: open addressing with linear
 * probing over a power-of-two table, FNV-1a string hashing. */
#include <errno.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static ENTRY *table;
static size_t mask, used;

int hcreate(size_t n)
{
	size_t cap = 8;
	if (n > SIZE_MAX / 4) {
		errno = ENOMEM;
		return 0;
	}
	while (cap < n + n / 4 + 1) cap *= 2;
	free(table);
	table = calloc(cap, sizeof *table);
	if (!table) return 0;
	mask = cap - 1;
	used = 0;
	return 1;
}

void hdestroy(void)
{
	free(table);
	table = 0;
	mask = used = 0;
}

static size_t hash(const char *s)
{
	uint64_t h = 0xcbf29ce484222325ULL;
	for (; *s; s++) h = (h ^ (unsigned char)*s) * 0x100000001b3ULL;
	return (size_t)h;
}

ENTRY *hsearch(ENTRY item, ACTION action)
{
	if (!table) {
		errno = ESRCH;
		return 0;
	}
	size_t i = hash(item.key) & mask;
	for (; table[i].key; i = (i + 1) & mask)
		if (!strcmp(table[i].key, item.key)) return &table[i];
	if (action == FIND) {
		errno = ESRCH;
		return 0;
	}
	if (used >= mask) {
		errno = ENOMEM;
		return 0;
	}
	table[i] = item;
	used++;
	return &table[i];
}
