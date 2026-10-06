/*
 * lib-spfxd — dl* interface in statically linked programs.
 *
 * A static executable has no dynamic linker, so dlopen cannot load
 * objects (a genuine limitation, reported through dlerror).  dlopen(NULL)
 * still returns a handle for the program; dlsym on it fails because a
 * static executable carries no dynamic symbol table.  dl_iterate_phdr
 * reports the program itself (the unwinder in libgcc needs this for C++
 * exceptions in static programs), and __tls_get_addr serves code that was
 * compiled for the general-dynamic TLS model.
 *
 * libc.so provides the real implementations (ldso/dynlink.c).
 */
#ifndef SPFXD_SHARED
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <stdint.h>
#include <string.h>
#include "pthread_impl.h"

hidden void __dl_seterr(const char *fmt, ...);
void *__tls_get_addr(size_t *);

static char self_handle;

extern const size_t _DYNAMIC[] __attribute__((__weak__, __visibility__("hidden")));

/* Program headers and load bias of the (static or static-PIE) program.
 * Static PIE executables usually have no PT_PHDR; their bias then comes
 * from the run-time address of _DYNAMIC. */
static const Elf64_Phdr *prog_phdrs(size_t *phnum, size_t *base)
{
	const Elf64_Phdr *ph = 0;
	*phnum = 0;
	*base = 0;
	for (size_t *a = __libc.auxv; a && a[0]; a += 2) {
		if (a[0] == AT_PHDR) ph = (const void *)a[1];
		else if (a[0] == AT_PHNUM) *phnum = a[1];
	}
	for (size_t i = 0; ph && i < *phnum; i++) {
		if (ph[i].p_type == PT_PHDR) {
			*base = (size_t)ph - ph[i].p_vaddr;
			return ph;
		}
	}
	for (size_t i = 0; ph && i < *phnum; i++)
		if (ph[i].p_type == PT_DYNAMIC && _DYNAMIC) *base = (size_t)_DYNAMIC - ph[i].p_vaddr;
	return ph;
}

void *dlopen(const char *file, int mode)
{
	(void)mode;
	if (!file) return &self_handle;
	__dl_seterr("%s: dynamic loading is not supported in statically linked programs", file);
	return 0;
}

int dlclose(void *h)
{
	if (h == &self_handle) return 0;
	__dl_seterr("invalid handle %p passed to dlclose", h);
	return -1;
}

void *dlsym(void *restrict h, const char *restrict s)
{
	(void)h;
	__dl_seterr("symbol not found: %s (statically linked program)", s);
	return 0;
}

int dladdr(const void *addr, Dl_info *info)
{
	(void)addr;
	(void)info;
	return 0;
}

int dl_iterate_phdr(int (*cb)(struct dl_phdr_info *, size_t, void *), void *data)
{
	size_t phnum, base;
	const Elf64_Phdr *ph = prog_phdrs(&phnum, &base);
	struct dl_phdr_info info;
	memset(&info, 0, sizeof info);
	for (size_t i = 0; ph && i < phnum; i++) {
		if (ph[i].p_type == PT_TLS) {
			info.dlpi_tls_modid = 1;
			info.dlpi_tls_data = (void *)__self()->dtv[1];
		}
	}
	info.dlpi_addr = base;
	info.dlpi_name = "";
	info.dlpi_phdr = ph;
	info.dlpi_phnum = (Elf64_Half)phnum;
	info.dlpi_adds = 1;
	info.dlpi_subs = 0;
	return cb(&info, sizeof info, data);
}

int _dl_find_object(void *pc, struct dl_find_object *res)
{
	size_t phnum, base, lo = SIZE_MAX, hi = 0;
	const Elf64_Phdr *ph = prog_phdrs(&phnum, &base);
	void *eh = 0;
	for (size_t i = 0; ph && i < phnum; i++) {
		if (ph[i].p_type == PT_LOAD) {
			if (ph[i].p_vaddr < lo) lo = ph[i].p_vaddr;
			if (ph[i].p_vaddr + ph[i].p_memsz > hi) hi = ph[i].p_vaddr + ph[i].p_memsz;
		} else if (ph[i].p_type == PT_GNU_EH_FRAME) {
			eh = (void *)(base + ph[i].p_vaddr);
		}
	}
	if (!ph || (size_t)pc < base + lo || (size_t)pc >= base + hi) return -1;
	memset(res, 0, sizeof *res);
	res->dlfo_map_start = (void *)(base + lo);
	res->dlfo_map_end = (void *)(base + hi);
	res->dlfo_eh_frame = eh;
	return 0;
}

void *__tls_get_addr(size_t *ti)
{
	return (char *)__self()->dtv[ti[0]] + ti[1];
}
#endif
