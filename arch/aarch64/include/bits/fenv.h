/* lib-spfxd — bits/fenv.h (AArch64: FPCR control, FPSR status) */
#ifndef _BITS_FENV_H
#define _BITS_FENV_H

#define FE_INVALID    0x01
#define FE_DIVBYZERO  0x02
#define FE_OVERFLOW   0x04
#define FE_UNDERFLOW  0x08
#define FE_INEXACT    0x10
#define FE_ALL_EXCEPT 0x1f

/* FPCR.RMode, bits 22-23 */
#define FE_TONEAREST  0x000000
#define FE_UPWARD     0x400000
#define FE_DOWNWARD   0x800000
#define FE_TOWARDZERO 0xc00000

typedef unsigned int fexcept_t;

typedef struct {
	unsigned int __fpcr;
	unsigned int __fpsr;
} fenv_t;

#define FE_DFL_ENV ((const fenv_t *)-1)

#endif
