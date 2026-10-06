/* lib-spfxd — bits/fenv.h (x86-64: x87 + SSE) */
#ifndef _BITS_FENV_H
#define _BITS_FENV_H

#define FE_INVALID    0x01
#define __FE_DENORM   0x02
#define FE_DIVBYZERO  0x04
#define FE_OVERFLOW   0x08
#define FE_UNDERFLOW  0x10
#define FE_INEXACT    0x20
#define FE_ALL_EXCEPT 0x3f

#define FE_TONEAREST  0x000
#define FE_DOWNWARD   0x400
#define FE_UPWARD     0x800
#define FE_TOWARDZERO 0xc00

typedef unsigned short fexcept_t;

/* The first 28 bytes are the x87 environment as stored by fnstenv; the
 * last field is MXCSR. */
typedef struct {
	unsigned short __control_word, __unused1;
	unsigned short __status_word, __unused2;
	unsigned short __tags, __unused3;
	unsigned int __eip;
	unsigned short __cs_selector;
	unsigned int __opcode:11, __unused4:5;
	unsigned int __data_offset;
	unsigned short __data_selector, __unused5;
	unsigned int __mxcsr;
} fenv_t;

#define FE_DFL_ENV ((const fenv_t *)-1)

#endif
