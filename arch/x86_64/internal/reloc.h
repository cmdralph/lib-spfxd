/* lib-spfxd — x86-64 dynamic relocation types, under the names the
 * dynamic linker uses for every architecture. */
#ifndef _SPFXD_RELOC_H
#define _SPFXD_RELOC_H
#include <elf.h>

#define ARCH_NAME     "x86_64"
#define ELF_MACHINE   EM_X86_64
#define REL_NONE      R_X86_64_NONE
#define REL_SYMBOLIC  R_X86_64_64
#define REL_GOT       R_X86_64_GLOB_DAT
#define REL_PLT       R_X86_64_JUMP_SLOT
#define REL_RELATIVE  R_X86_64_RELATIVE
#define REL_COPY      R_X86_64_COPY
#define REL_DTPMOD    R_X86_64_DTPMOD64
#define REL_DTPOFF    R_X86_64_DTPOFF64
#define REL_TPOFF     R_X86_64_TPOFF64
#define REL_IRELATIVE R_X86_64_IRELATIVE
/* x86-64 only */
#define REL_PC32      R_X86_64_PC32
#define REL_32        R_X86_64_32
#define REL_32S       R_X86_64_32S
#define REL_SIZE64    R_X86_64_SIZE64

#endif
