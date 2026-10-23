/* lib-spfxd — bits/fenv.h (RISC-V: fcsr = frm << 5 | fflags) */
#ifndef _BITS_FENV_H
#define _BITS_FENV_H

/* fflags */
#define FE_INEXACT    0x01
#define FE_UNDERFLOW  0x02
#define FE_OVERFLOW   0x04
#define FE_DIVBYZERO  0x08
#define FE_INVALID    0x10
#define FE_ALL_EXCEPT 0x1f

/* frm encodings (RNE, RTZ, RDN, RUP) */
#define FE_TONEAREST  0
#define FE_TOWARDZERO 1
#define FE_DOWNWARD   2
#define FE_UPWARD     3

typedef unsigned int fexcept_t;
typedef unsigned int fenv_t;     /* the whole fcsr */

#define FE_DFL_ENV ((const fenv_t *)-1)

#endif
