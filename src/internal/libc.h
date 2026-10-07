/*
 * lib-spfxd — internal library-wide definitions.
 *
 * Conventions:
 *   hidden        symbol is internal to the library (never exported from
 *                 libc.so and never interposable)
 *   weak_alias    provide an alternate (usually public) name for a function
 *   __libc        the single structure of process-wide runtime state
 */
#ifndef _SPFXD_LIBC_H
#define _SPFXD_LIBC_H

#include <stddef.h>
#include <stdint.h>
#include <features.h>
#include "arch.h"

#define hidden __attribute__((__visibility__("hidden")))
#define weak __attribute__((__weak__))
#define weak_alias(old, new) \
	extern __typeof(old) new __attribute__((__weak__, __alias__(#old)))
#define strong_alias(old, new) \
	extern __typeof(old) new __attribute__((__alias__(#old)))
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define noinline __attribute__((__noinline__))
#define always_inline __inline __attribute__((__always_inline__))
#define cold __attribute__((__cold__))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* One TLS segment (the executable's or a loaded library's). */
struct tls_module {
	struct tls_module *next;
	const void *image;     /* initialization image (.tdata) */
	size_t len;            /* bytes of image to copy */
	size_t size;           /* total size including .tbss */
	size_t align;
	size_t offset;         /* distance below the thread pointer (variant II) */
};

struct libc_state {
	volatile int threaded;     /* a second thread has ever been created */
	volatile int can_cancel;   /* pthread_cancel has been called at least once */
	int secure;                /* AT_SECURE / setuid-style process */
	int initialized;           /* runtime set up (by the dynamic linker if dynamic) */
	int dynamic;               /* started through the dynamic linker */
	size_t page_size;
	size_t *auxv;
	struct tls_module *tls_head;
	size_t tls_size;           /* bytes needed per thread for dtv+TLS+TCB */
	size_t tls_align;
	size_t tls_cnt;            /* number of modules with static TLS */
	size_t default_stack;      /* default thread stack size */
	size_t default_guard;
	unsigned long hwcap;
	const char *progname;
	uintptr_t secret;          /* per-process random value for pointer hardening */
};

extern hidden struct libc_state __libc;
extern char **__environ;

/* ABI entry points with no public header declaration */
int __libc_start_main(int (*)(int, char **, char **), int, char **,
	void (*)(void), void (*)(void), void (*)(void));
int __cxa_atexit(void (*)(void *), void *, void *);
void __cxa_finalize(void *);
void __stack_chk_fail(void);
hidden void __stack_chk_fail_local(void);
hidden void __tls_layout_finish(size_t max_offset);
hidden void __init_tls_dynamic(void);
void *__tls_get_addr(size_t *);

hidden void __init_libc(char **envp, char *progname);
hidden void __init_tls(size_t *aux);
hidden void *__copy_tls(unsigned char *mem);
hidden void __init_ssp(void *entropy);
hidden void __init_cpu(void);
extern hidden unsigned __cpu_features;
hidden void __funcs_on_exit(void);
hidden void __funcs_on_quick_exit(void);
hidden void __libc_exit_fini(void);
hidden void __stdio_exit(void);
hidden void __env_rm_add(char *old, char *new);
hidden void *__vdsosym(const char *ver, const char *name);
hidden void __init_vdso(void);
hidden __attribute__((__noreturn__)) void __libc_fatal(const char *msg);
hidden size_t __libc_strlen_safe(const char *s);

#include "proto.h"

#endif
