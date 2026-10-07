/*
 * lib-spfxd — tsearch / tfind / tdelete / twalk / tdestroy as an AVL tree,
 * so all operations are O(log n) regardless of insertion order.  The node
 * layout starts with the key pointer, as POSIX requires (callers
 * dereference the returned node pointer to get the key).
 */
#include <search.h>
#include <stdlib.h>

struct node {
	const void *key;
	struct node *a[2];
	int h;
};

static int height(struct node *n) { return n ? n->h : 0; }

static void fix(struct node *n)
{
	int l = height(n->a[0]), r = height(n->a[1]);
	n->h = (l > r ? l : r) + 1;
}

static struct node *rotate(struct node *n, int dir)
{
	struct node *c = n->a[!dir];
	n->a[!dir] = c->a[dir];
	c->a[dir] = n;
	fix(n);
	fix(c);
	return c;
}

static struct node *balance(struct node *n)
{
	fix(n);
	int d = height(n->a[0]) - height(n->a[1]);
	if (d > 1) {
		if (height(n->a[0]->a[0]) < height(n->a[0]->a[1])) n->a[0] = rotate(n->a[0], 0);
		return rotate(n, 1);
	}
	if (d < -1) {
		if (height(n->a[1]->a[1]) < height(n->a[1]->a[0])) n->a[1] = rotate(n->a[1], 1);
		return rotate(n, 0);
	}
	return n;
}

static struct node *insert(struct node *n, const void *key, int (*cmp)(const void *, const void *),
	struct node **found, int *oom)
{
	if (!n) {
		struct node *m = malloc(sizeof *m);
		if (!m) {
			*oom = 1;
			return 0;
		}
		m->key = key;
		m->a[0] = m->a[1] = 0;
		m->h = 1;
		*found = m;
		return m;
	}
	int c = cmp(key, n->key);
	if (!c) {
		*found = n;
		return n;
	}
	struct node *sub = insert(n->a[c > 0], key, cmp, found, oom);
	if (*oom && !sub) return n;
	n->a[c > 0] = sub;
	return balance(n);
}

void *tsearch(const void *key, void **root, int (*cmp)(const void *, const void *))
{
	struct node *found = 0;
	int oom = 0;
	if (!root) return 0;
	struct node *r = insert(*root, key, cmp, &found, &oom);
	if (oom) return 0;
	*root = r;
	return found;
}

void *tfind(const void *key, void *const *root, int (*cmp)(const void *, const void *))
{
	if (!root) return 0;
	struct node *n = *root;
	while (n) {
		int c = cmp(key, n->key);
		if (!c) return n;
		n = n->a[c > 0];
	}
	return 0;
}

static struct node *unlink_min(struct node *n, struct node **min)
{
	if (!n->a[0]) {
		*min = n;
		return n->a[1];
	}
	n->a[0] = unlink_min(n->a[0], min);
	return balance(n);
}

static struct node *remove_key(struct node *n, const void *key, int (*cmp)(const void *, const void *),
	struct node **parent, struct node *up, int *found)
{
	if (!n) return 0;
	int c = cmp(key, n->key);
	if (c) {
		n->a[c > 0] = remove_key(n->a[c > 0], key, cmp, parent, n, found);
		return balance(n);
	}
	*found = 1;
	*parent = up;
	struct node *l = n->a[0], *r = n->a[1];
	free(n);
	if (!r) return l;
	struct node *m;
	r = unlink_min(r, &m);
	m->a[0] = l;
	m->a[1] = r;
	return balance(m);
}

void *tdelete(const void *restrict key, void **restrict root, int (*cmp)(const void *, const void *))
{
	struct node *parent = 0;
	int found = 0;
	if (!root || !*root) return 0;
	struct node *r = remove_key(*root, key, cmp, &parent, 0, &found);
	if (!found) return 0;
	*root = r;
	/* POSIX: return the parent of the deleted node (any non-null value
	 * when the root itself was deleted) */
	return parent ? (void *)parent : (void *)root;
}

static void walk(const struct node *n, void (*action)(const void *, VISIT, int), int depth)
{
	if (!n) return;
	if (!n->a[0] && !n->a[1]) {
		action(n, leaf, depth);
		return;
	}
	action(n, preorder, depth);
	walk(n->a[0], action, depth + 1);
	action(n, postorder, depth);
	walk(n->a[1], action, depth + 1);
	action(n, endorder, depth);
}

void twalk(const void *root, void (*action)(const void *, VISIT, int))
{
	walk(root, action, 0);
}

void tdestroy(void *root, void (*freekey)(void *))
{
	struct node *n = root;
	if (!n) return;
	tdestroy(n->a[0], freekey);
	tdestroy(n->a[1], freekey);
	if (freekey) freekey((void *)n->key);
	free(n);
}
