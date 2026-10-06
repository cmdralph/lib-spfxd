/*
 * lib-spfxd — rcrt1.o: entry point of static PIE executables.
 *
 * Identical to crt1.o except that the executable first relocates itself:
 * it is position independent but there is no dynamic linker to apply its
 * RELATIVE relocations.
 */
#include "reloc_self.h"

#define START_C_ATTR static
#define RCRT1_SELF_RELOCATE(p, dyn) do { \
	size_t *auxv_ = (size_t *)((p) + 1 + (p)[0] + 1); \
	while (*auxv_) auxv_++; \
	auxv_++; \
	size_t base_ = __rs_base_from_phdr((const Elf64_Phdr *)__rs_aux(auxv_, AT_PHDR), \
		__rs_aux(auxv_, AT_PHNUM), __rs_aux(auxv_, AT_PHENT), (size_t)(dyn)); \
	__rs_relocate(base_, (const Elf64_Dyn *)(dyn), 0); \
} while (0)

#include "crt1.c"
