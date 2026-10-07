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

__asm__(
	".text\n"
	".global _dlstart\n"
	".hidden _dlstart\n"
	".type _dlstart,@function\n"
	"_dlstart:\n"
	"	xor %ebp,%ebp\n"
	"	mov %rsp,%rdi\n"
	"	lea _DYNAMIC(%rip),%rsi\n"
	"	and $-16,%rsp\n"
	"	call _dlstart_c\n"
	"	hlt\n"
	".size _dlstart,.-_dlstart\n"
);

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
