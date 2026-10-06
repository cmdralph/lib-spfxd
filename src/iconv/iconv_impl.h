/* lib-spfxd — iconv internals */
#ifndef _SPFXD_ICONV_IMPL_H
#define _SPFXD_ICONV_IMPL_H
#include <stdint.h>
#include "libc.h"

struct sbcs {
	const char *name;
	const char *aliases;             /* NUL-separated, double-NUL terminated */
	const uint16_t *high;            /* bytes 0x80..0xff */
};

extern hidden const struct sbcs __iconv_sbcs[];

#endif
