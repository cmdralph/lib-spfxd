/*
 * lib-spfxd — dynamic linker entry (stage 1).
 *
 * libc.so is its own dynamic linker: the kernel maps it as the program
 * interpreter (or runs it directly as `libc.so prog args`) and enters
 * _dlstart.  Stage 1 relocates libc.so against itself using only code
 * that needs no relocation (see reloc_self.h), then calls stage 2 in
 * dynlink.c, which can use the rest of the library.
 */
#include "libc.h"
#include "reloc_self.h"

hidden _Noreturn void __dls2(unsigned char *base, size_t *sp);

/* the entry stub is the same as a program's _start (crt_arch.h): pass the
 * initial stack pointer and the address of _DYNAMIC to _dlstart_c */
#define START "_dlstart"
#define START_EXTRA ".hidden _dlstart\n"
#include "crt_arch.h"

hidden _Noreturn void _dlstart_c(size_t *sp, size_t *dynv);

hidden _Noreturn void _dlstart_c(size_t *sp, size_t *dynv)
{
	size_t argc = sp[0];
	size_t *auxv = sp + 1 + argc + 1;
	while (*auxv) auxv++;
	auxv++;

	size_t base = __rs_aux(auxv, AT_BASE);
	if (!base) {
		/* run directly: the kernel's AT_PHDR describes libc.so itself */
		base = __rs_base_from_phdr((const Elf64_Phdr *)__rs_aux(auxv, AT_PHDR),
			__rs_aux(auxv, AT_PHNUM), __rs_aux(auxv, AT_PHENT), (size_t)dynv);
	}
	__rs_relocate(base, (const Elf64_Dyn *)dynv, 1);
	__dls2((unsigned char *)base, sp);
}
