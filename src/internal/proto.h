/*
 * lib-spfxd — prototypes of internal functions shared between subsystems
 * that have no better home.  Everything here is hidden (library-private).
 */
#ifndef _SPFXD_PROTO_H
#define _SPFXD_PROTO_H

#include <stddef.h>
#include <stdint.h>

struct sigaction;

/* stdlib */
hidden uintmax_t __strto_core(const char *, char **, int, uintmax_t, uintmax_t, int *, int *);
hidden const unsigned char *__digit_values(void);

/* errno */
hidden const char *__strerror_lookup(int);
char *__gnu_strerror_r(int, char *, size_t);
int __xpg_strerror_r(int, char *, size_t);

/* signals */
hidden int __libc_sigaction(int, const struct sigaction *, struct sigaction *);

/* Unicode tables */
hidden int __uni_alpha(uint32_t);
hidden int __uni_upper(uint32_t);
hidden int __uni_lower(uint32_t);
hidden int __uni_space(uint32_t);
hidden int __uni_cntrl(uint32_t);
hidden int __uni_punct(uint32_t);
hidden int __uni_graph(uint32_t);
hidden int __uni_print(uint32_t);
hidden uint32_t __uni_toupper(uint32_t);
hidden uint32_t __uni_tolower(uint32_t);
hidden int __uni_width(uint32_t);

#endif
