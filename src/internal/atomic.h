/*
 * lib-spfxd — atomic primitives for library internals.
 *
 * Built on the compiler's __atomic builtins (the same machinery as C11
 * <stdatomic.h>), which on x86-64 compile to plain loads/stores, lock-prefixed
 * read-modify-write instructions and xchg.  Every read-modify-write here is
 * sequentially consistent: they guard futex protocols where the cost of a
 * weaker ordering bug far outweighs the (on x86, zero) difference in cost.
 */
#ifndef _SPFXD_ATOMIC_H
#define _SPFXD_ATOMIC_H

#include "arch.h"

#define A_SC __ATOMIC_SEQ_CST

/* Compare-and-swap; returns the value that was in *p before the operation. */
static __inline int a_cas(volatile int *p, int expect, int desired)
{
	__atomic_compare_exchange_n(p, &expect, desired, 0, A_SC, A_SC);
	return expect;
}
static __inline void *a_cas_p(void *volatile *p, void *expect, void *desired)
{
	__atomic_compare_exchange_n(p, &expect, desired, 0, A_SC, A_SC);
	return expect;
}
static __inline unsigned long a_cas_l(volatile unsigned long *p, unsigned long expect, unsigned long desired)
{
	__atomic_compare_exchange_n(p, &expect, desired, 0, A_SC, A_SC);
	return expect;
}
static __inline int a_swap(volatile int *p, int v) { return __atomic_exchange_n(p, v, A_SC); }
static __inline void *a_swap_p(void *volatile *p, void *v) { return __atomic_exchange_n(p, v, A_SC); }
static __inline int a_fetch_add(volatile int *p, int v) { return __atomic_fetch_add(p, v, A_SC); }
static __inline unsigned long a_fetch_add_l(volatile unsigned long *p, unsigned long v)
{ return __atomic_fetch_add(p, v, A_SC); }
static __inline int a_fetch_or(volatile int *p, int v) { return __atomic_fetch_or(p, v, A_SC); }
static __inline int a_fetch_and(volatile int *p, int v) { return __atomic_fetch_and(p, v, A_SC); }
static __inline void a_inc(volatile int *p) { __atomic_fetch_add(p, 1, A_SC); }
static __inline void a_dec(volatile int *p) { __atomic_fetch_sub(p, 1, A_SC); }
static __inline void a_store(volatile int *p, int v) { __atomic_store_n(p, v, A_SC); }
static __inline void a_store_rel(volatile int *p, int v) { __atomic_store_n(p, v, __ATOMIC_RELEASE); }
static __inline int a_load(volatile int *p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }
static __inline void *a_load_p(void *volatile *p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }
static __inline void a_barrier(void) { __atomic_thread_fence(A_SC); }
static __inline void a_spin(void) { __arch_spin(); }
static __inline __attribute__((__noreturn__)) void a_crash(void) { __arch_crash(); }

static __inline int a_ctz_64(unsigned long x) { return __builtin_ctzl(x); }
static __inline int a_clz_64(unsigned long x) { return __builtin_clzl(x); }
static __inline int a_ctz_32(unsigned x) { return __builtin_ctz(x); }
static __inline int a_clz_32(unsigned x) { return __builtin_clz(x); }

#endif
