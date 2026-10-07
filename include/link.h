/* lib-spfxd — <link.h> */
#ifndef _LINK_H
#define _LINK_H
#include <features.h>
#include <elf.h>
#define __SPFXD_NEED_size_t
#include <bits/typedefs.h>

#define ElfW(type) Elf64_ ## type

struct link_map {
	ElfW(Addr) l_addr;
	char *l_name;
	ElfW(Dyn) *l_ld;
	struct link_map *l_next, *l_prev;
};

struct r_debug {
	int r_version;
	struct link_map *r_map;
	ElfW(Addr) r_brk;
	enum { RT_CONSISTENT, RT_ADD, RT_DELETE } r_state;
	ElfW(Addr) r_ldbase;
};

struct dl_phdr_info {
	ElfW(Addr) dlpi_addr;
	const char *dlpi_name;
	const ElfW(Phdr) *dlpi_phdr;
	ElfW(Half) dlpi_phnum;
	unsigned long long dlpi_adds;
	unsigned long long dlpi_subs;
	size_t dlpi_tls_modid;
	void *dlpi_tls_data;
};

__SPFXD_BEGIN_DECLS
int dl_iterate_phdr(int (*)(struct dl_phdr_info *, size_t, void *), void *);
__SPFXD_END_DECLS
#endif
