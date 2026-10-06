/* lib-spfxd — lsearch / lfind / insque / remque. */
#include <search.h>
#include <string.h>

void *lfind(const void *key, const void *base, size_t *nel, size_t w, int (*cmp)(const void *, const void *))
{
	const char *p = base;
	for (size_t i = 0; i < *nel; i++, p += w)
		if (!cmp(key, p)) return (void *)p;
	return 0;
}

void *lsearch(const void *key, void *base, size_t *nel, size_t w, int (*cmp)(const void *, const void *))
{
	void *r = lfind(key, base, nel, w, cmp);
	if (r) return r;
	r = (char *)base + *nel * w;
	memcpy(r, key, w);
	++*nel;
	return r;
}

struct qelem { struct qelem *next, *prev; };

void insque(void *e, void *pred)
{
	struct qelem *el = e, *p = pred;
	if (!p) {
		el->next = el->prev = 0;
		return;
	}
	el->next = p->next;
	el->prev = p;
	p->next = el;
	if (el->next) el->next->prev = el;
}

void remque(void *e)
{
	struct qelem *el = e;
	if (el->next) el->next->prev = el->prev;
	if (el->prev) el->prev->next = el->next;
}
