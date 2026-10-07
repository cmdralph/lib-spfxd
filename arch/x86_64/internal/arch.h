/*
 * lib-spfxd — x86-64 architecture hooks used by portable code.
 */
#ifndef _SPFXD_ARCH_H
#define _SPFXD_ARCH_H

/* Thread pointer: %fs base holds the address of the current thread's
 * struct pthread, whose first word points to itself (x86-64 TLS ABI). */
static __inline void *__arch_tp(void)
{
	void *p;
	__asm__ ("mov %%fs:0,%0" : "=r"(p));
	return p;
}

/* TLS variant II: TLS blocks live below the thread pointer. */
#define TLS_VARIANT_2 1
#define TLS_ABOVE_TP 0

/* Stack protector canary offset fixed by the GCC/Clang x86-64 ABI: %fs:0x28. */
#define TCB_CANARY_OFFSET 0x28

#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003

/* CPU spin-wait hint. */
static __inline void __arch_spin(void) { __asm__ __volatile__ ("pause" ::: "memory"); }

/* Deliberate crash for unrecoverable internal corruption: hlt in user mode
 * raises SIGSEGV immediately, without touching memory or the stack. */
static __inline __attribute__((__noreturn__)) void __arch_crash(void)
{
	for (;;) __asm__ __volatile__ ("hlt" ::: "memory");
}

#define ARCH_PAGE_SIZE 4096UL
#define ARCH_CACHE_LINE 64

#endif
