/* lib-spfxd — bzero / bcopy / explicit_bzero. */
#include <strings.h>
#include <string.h>

void bzero(void *s, size_t n) { memset(s, 0, n); }
void bcopy(const void *s, void *d, size_t n) { memmove(d, s, n); }

/* The empty asm with a memory clobber makes the stores observable, so the
 * compiler cannot drop them as dead. */
void explicit_bzero(void *s, size_t n)
{
	memset(s, 0, n);
	__asm__ __volatile__ ("" : : "r"(s) : "memory");
}
