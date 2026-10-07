/* lib-spfxd — strspn / strcspn / strpbrk using a 256-bit membership map. */
#include <string.h>
#include <stdint.h>

#define BIT(m, c) ((m)[(unsigned char)(c) >> 6] & (1ULL << ((unsigned char)(c) & 63)))
#define SET(m, c) ((m)[(unsigned char)(c) >> 6] |= (1ULL << ((unsigned char)(c) & 63)))

size_t strspn(const char *s, const char *accept)
{
	const char *a = s;
	uint64_t map[4] = { 0 };
	if (!accept[0]) return 0;
	if (!accept[1]) {
		for (; *s == *accept; s++);
		return (size_t)(s - a);
	}
	for (; *accept; accept++) SET(map, *accept);
	for (; *s && BIT(map, *s); s++);
	return (size_t)(s - a);
}

size_t strcspn(const char *s, const char *reject)
{
	const char *a = s;
	uint64_t map[4] = { 0 };
	if (!reject[0] || !reject[1]) return (size_t)(strchrnul(s, *reject) - s);
	for (; *reject; reject++) SET(map, *reject);
	for (; *s && !BIT(map, *s); s++);
	return (size_t)(s - a);
}

char *strpbrk(const char *s, const char *b)
{
	s += strcspn(s, b);
	return *s ? (char *)s : 0;
}
