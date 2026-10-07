/*
 * lib-spfxd — internal system call layer.
 *
 *   __syscall(nr, args...)     raw call; returns the kernel value (-errno on failure)
 *   __sysret(nr, args...)      call and translate failures to errno / -1
 *   __syscall_cp / __sysret_cp the same, but acting as a thread
 *                              cancellation point
 *
 * Arguments are converted to long at the call site so pointers and
 * integers of any width are passed correctly in 64-bit registers.
 */
#ifndef _SPFXD_SYSCALL_H
#define _SPFXD_SYSCALL_H

#include <bits/syscall.h>
#include "syscall_arch.h"
#include "libc.h"

hidden long __syscall_ret(unsigned long r);
hidden long __syscall_cp_c(long nr, long a, long b, long c, long d, long e, long f);

#define __scc(x) ((long)(x))

#define __sc_narg_x(a, b, c, d, e, f, g, h, n, ...) n
#define __sc_narg(...) __sc_narg_x(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0, )
#define __sc_cat_x(a, b) a##b
#define __sc_cat(a, b) __sc_cat_x(a, b)
#define __sc_disp(b, ...) __sc_cat(b, __sc_narg(__VA_ARGS__))(__VA_ARGS__)

#define __sc0(n)                   __syscall0(n)
#define __sc1(n, a)                __syscall1(n, __scc(a))
#define __sc2(n, a, b)             __syscall2(n, __scc(a), __scc(b))
#define __sc3(n, a, b, c)          __syscall3(n, __scc(a), __scc(b), __scc(c))
#define __sc4(n, a, b, c, d)       __syscall4(n, __scc(a), __scc(b), __scc(c), __scc(d))
#define __sc5(n, a, b, c, d, e)    __syscall5(n, __scc(a), __scc(b), __scc(c), __scc(d), __scc(e))
#define __sc6(n, a, b, c, d, e, f) __syscall6(n, __scc(a), __scc(b), __scc(c), __scc(d), __scc(e), __scc(f))

#define __syscall(...) __sc_disp(__sc, __VA_ARGS__)
#define __sysret(...) __syscall_ret(__syscall(__VA_ARGS__))

#define __scp0(n)                   __syscall_cp_c(n, 0, 0, 0, 0, 0, 0)
#define __scp1(n, a)                __syscall_cp_c(n, __scc(a), 0, 0, 0, 0, 0)
#define __scp2(n, a, b)             __syscall_cp_c(n, __scc(a), __scc(b), 0, 0, 0, 0)
#define __scp3(n, a, b, c)          __syscall_cp_c(n, __scc(a), __scc(b), __scc(c), 0, 0, 0)
#define __scp4(n, a, b, c, d)       __syscall_cp_c(n, __scc(a), __scc(b), __scc(c), __scc(d), 0, 0)
#define __scp5(n, a, b, c, d, e)    __syscall_cp_c(n, __scc(a), __scc(b), __scc(c), __scc(d), __scc(e), 0)
#define __scp6(n, a, b, c, d, e, f) __syscall_cp_c(n, __scc(a), __scc(b), __scc(c), __scc(d), __scc(e), __scc(f))

#define __syscall_cp(...) __sc_disp(__scp, __VA_ARGS__)
#define __sysret_cp(...) __syscall_ret(__syscall_cp(__VA_ARGS__))

/* Fast, inline errno translation for wrappers that care. */
#define __is_err(r) ((unsigned long)(r) > -4096UL)

#endif
