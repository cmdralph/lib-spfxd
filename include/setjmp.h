/* lib-spfxd — <setjmp.h> */
#ifndef _SETJMP_H
#define _SETJMP_H
#include <features.h>
#include <bits/setjmp.h>

__SPFXD_BEGIN_DECLS

typedef struct __spfxd_jmp_buf_tag {
	__spfxd_jmp_regs __jb;      /* callee-saved registers, stack and return address */
	unsigned long __fl;         /* nonzero: __ss holds a saved signal mask */
	unsigned long __ss[128 / sizeof(long)];
} jmp_buf[1];

int setjmp(jmp_buf) __attribute__((__returns_twice__));
__spfxd_noreturn void longjmp(jmp_buf, int);
#define setjmp setjmp

#if defined(__SPFXD_POSIX)
typedef jmp_buf sigjmp_buf;
int sigsetjmp(sigjmp_buf, int) __attribute__((__returns_twice__));
__spfxd_noreturn void siglongjmp(sigjmp_buf, int);
#endif
#if defined(__SPFXD_XSI) || defined(__SPFXD_BSD)
int _setjmp(jmp_buf) __attribute__((__returns_twice__));
__spfxd_noreturn void _longjmp(jmp_buf, int);
#endif

__SPFXD_END_DECLS
#endif
