/* lib-spfxd — strlen (portable): aligned word scan for the terminator. */
#include <string.h>
#include "string_impl.h"

size_t (strlen)(const char *s)
{
	const char *a = s;
	for (; !ALIGNED(s); s++)
		if (!*s) return (size_t)(s - a);
	const word_t *w = (const word_t *)s;
	size_t z;
	while (!(z = HASZERO(*w))) w++;
	return (size_t)((const char *)w - a) + __zero_byte_index(z);
}
