/*
 * lib-spfxd — vDSO symbol lookup.
 *
 * The kernel maps a small shared object (the vDSO) into every process and
 * passes its address in AT_SYSINFO_EHDR.  Its clock_gettime/gettimeofday/
 * time implementations read the kernel's timekeeping data from user space,
 * avoiding a system call.  This walks the vDSO's dynamic symbol table,
 * matching the symbol name and version.
 */
#include <elf.h>
#include <string.h>
#include "libc.h"

static size_t count_syms(const uint32_t *hashtab, const uint32_t *gnuhash)
{
	if (hashtab) return hashtab[1];
	if (!gnuhash) return 0;
	/* GNU hash: the highest symbol index reachable from any bucket */
	uint32_t nbuckets = gnuhash[0], symoffset = gnuhash[1], bloom = gnuhash[2];
	const uint32_t *buckets = gnuhash + 4 + bloom * 2;
	const uint32_t *chain = buckets + nbuckets;
	uint32_t last = 0;
	for (uint32_t i = 0; i < nbuckets; i++)
		if (buckets[i] > last) last = buckets[i];
	if (last < symoffset) return symoffset;
	while (!(chain[last - symoffset] & 1)) last++;
	return last + 1;
}

static int version_matches(const uint16_t *versym, size_t i, const Elf64_Verdef *def,
	const char *strings, const char *want)
{
	if (!versym) return 1;
	uint16_t v = versym[i] & 0x7fff;
	for (; ; def = (const Elf64_Verdef *)((const char *)def + def->vd_next)) {
		if (!(def->vd_flags & VER_FLG_BASE) && (def->vd_ndx & 0x7fff) == v) break;
		if (!def->vd_next) return 0;
	}
	const Elf64_Verdaux *aux = (const Elf64_Verdaux *)((const char *)def + def->vd_aux);
	return !strcmp(want, strings + aux->vda_name);
}

hidden void *__vdsosym(const char *vername, const char *name)
{
	size_t ehdr_addr = 0;
	for (size_t *a = __libc.auxv; a && *a; a += 2)
		if (*a == AT_SYSINFO_EHDR) ehdr_addr = a[1];
	if (!ehdr_addr) return 0;

	const Elf64_Ehdr *eh = (const void *)ehdr_addr;
	const Elf64_Phdr *ph = (const void *)(ehdr_addr + eh->e_phoff);
	const Elf64_Dyn *dyn = 0;
	size_t base = (size_t)-1;
	for (size_t i = 0; i < eh->e_phnum; i++, ph = (const void *)((const char *)ph + eh->e_phentsize)) {
		if (ph->p_type == PT_LOAD && base == (size_t)-1) base = ehdr_addr + ph->p_offset - ph->p_vaddr;
		else if (ph->p_type == PT_DYNAMIC) dyn = (const void *)(ehdr_addr + ph->p_offset);
	}
	if (!dyn || base == (size_t)-1) return 0;

	const char *strings = 0;
	const Elf64_Sym *syms = 0;
	const uint32_t *hashtab = 0, *gnuhash = 0;
	const uint16_t *versym = 0;
	const Elf64_Verdef *verdef = 0;
	for (; dyn->d_tag; dyn++) {
		const void *p = (const void *)(base + dyn->d_un.d_ptr);
		switch (dyn->d_tag) {
		case DT_STRTAB: strings = p; break;
		case DT_SYMTAB: syms = p; break;
		case DT_HASH: hashtab = p; break;
		case DT_GNU_HASH: gnuhash = p; break;
		case DT_VERSYM: versym = p; break;
		case DT_VERDEF: verdef = p; break;
		}
	}
	if (!strings || !syms) return 0;
	if (!verdef) versym = 0;
	size_t n = count_syms(hashtab, gnuhash);
	for (size_t i = 0; i < n; i++) {
		const Elf64_Sym *s = &syms[i];
		int type = ELF64_ST_TYPE(s->st_info), bind = ELF64_ST_BIND(s->st_info);
		if (type != STT_FUNC && type != STT_NOTYPE) continue;
		if (bind != STB_GLOBAL && bind != STB_WEAK) continue;
		if (!s->st_shndx) continue;
		if (strcmp(name, strings + s->st_name)) continue;
		if (!version_matches(versym, i, verdef, strings, vername)) continue;
		return (void *)(base + s->st_value);
	}
	return 0;
}
