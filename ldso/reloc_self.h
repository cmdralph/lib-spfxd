/*
 * lib-spfxd — self-relocation before anything else runs.
 *
 * Used by the dynamic linker's entry (ldso/dlstart.c) and by rcrt1.o for
 * static PIE executables.  At this point no relocation has been applied:
 * the code below may only use local variables and PC-relative addressing
 * (everything is static inline; no global data, no calls through the GOT,
 * no compiler-generated memset/memcpy for aggregates).
 *
 *   base   load bias: AT_BASE for the interpreter, otherwise derived from
 *          the program headers (PT_PHDR or PT_DYNAMIC) the kernel reports
 *   RELATIVE and RELR relocations are applied; with `self_syms`, symbolic
 *   GLOB_DAT/JUMP_SLOT/64 relocations are resolved against the object's own
 *   definitions as a provisional binding (the dynamic linker rebinds them
 *   later in the proper global lookup order).
 */
#ifndef _SPFXD_RELOC_SELF_H
#define _SPFXD_RELOC_SELF_H

#include <elf.h>
#include "reloc.h"
#include <stddef.h>
#include <stdint.h>

#define RS_DT_RELR    36
#define RS_DT_RELRSZ  35

static __inline __attribute__((__always_inline__))
size_t __rs_aux(const size_t *auxv, size_t type)
{
	for (; auxv[0]; auxv += 2)
		if (auxv[0] == type) return auxv[1];
	return 0;
}

/* Load bias of the object whose dynamic section is `dynv`, given the
 * program headers of that same object. */
static __inline __attribute__((__always_inline__))
size_t __rs_base_from_phdr(const Elf64_Phdr *ph, size_t phnum, size_t phent, size_t dynv)
{
	for (size_t i = 0; i < phnum; i++, ph = (const void *)((const char *)ph + phent))
		if (ph->p_type == PT_DYNAMIC) return dynv - ph->p_vaddr;
	return 0;
}

static __inline __attribute__((__always_inline__))
void __rs_relocate(size_t base, const Elf64_Dyn *dynv, int self_syms)
{
	size_t rela = 0, relasz = 0, jmprel = 0, pltrelsz = 0, relr = 0, relrsz = 0;
	size_t symtab = 0;
	for (; dynv->d_tag; dynv++) {
		switch (dynv->d_tag) {
		case DT_RELA: rela = dynv->d_un.d_ptr; break;
		case DT_RELASZ: relasz = dynv->d_un.d_val; break;
		case DT_JMPREL: jmprel = dynv->d_un.d_ptr; break;
		case DT_PLTRELSZ: pltrelsz = dynv->d_un.d_val; break;
		case RS_DT_RELR: relr = dynv->d_un.d_ptr; break;
		case RS_DT_RELRSZ: relrsz = dynv->d_un.d_val; break;
		case DT_SYMTAB: symtab = dynv->d_un.d_ptr; break;
		}
	}
	for (int pass = 0; pass < 2; pass++) {
		const Elf64_Rela *r = (const void *)(base + (pass ? jmprel : rela));
		size_t n = (pass ? pltrelsz : relasz) / sizeof *r;
		if (!(pass ? jmprel : rela)) continue;
		for (size_t i = 0; i < n; i++, r++) {
			size_t *where = (size_t *)(base + r->r_offset);
			uint32_t type = (uint32_t)ELF64_R_TYPE(r->r_info);
			uint32_t si = (uint32_t)ELF64_R_SYM(r->r_info);
			if (type == REL_RELATIVE) {
				*where = base + (size_t)r->r_addend;
			} else if (self_syms && si && symtab &&
			           (type == REL_GOT || type == REL_PLT || type == REL_SYMBOLIC)) {
				const Elf64_Sym *s = (const Elf64_Sym *)(base + symtab) + si;
				if (s->st_shndx != SHN_UNDEF && ELF64_ST_TYPE(s->st_info) != STT_TLS)
					*where = base + s->st_value + (type == REL_SYMBOLIC ? (size_t)r->r_addend : 0);
			}
		}
	}
	if (relr) {
		const size_t *e = (const void *)(base + relr);
		size_t n = relrsz / sizeof *e, *where = 0;
		for (size_t i = 0; i < n; i++) {
			if (!(e[i] & 1)) {
				where = (size_t *)(base + e[i]);
				*where++ += base;
			} else {
				size_t bits = e[i] >> 1;
				for (size_t k = 0; bits; k++, bits >>= 1)
					if (bits & 1) where[k] += base;
				where += 8 * sizeof(size_t) - 1;
			}
		}
	}
}

#endif
