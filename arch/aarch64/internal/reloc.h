/* lib-spfxd — AArch64 dynamic relocation types, under the names the
 * dynamic linker uses for every architecture. */
#ifndef _SPFXD_RELOC_H
#define _SPFXD_RELOC_H
#include <elf.h>

#define ARCH_NAME     "aarch64"
#define ELF_MACHINE   EM_AARCH64
#define REL_NONE      R_AARCH64_NONE
#define REL_SYMBOLIC  R_AARCH64_ABS64
#define REL_GOT       R_AARCH64_GLOB_DAT
#define REL_PLT       R_AARCH64_JUMP_SLOT
#define REL_RELATIVE  R_AARCH64_RELATIVE
#define REL_COPY      R_AARCH64_COPY
#define REL_DTPMOD    R_AARCH64_TLS_DTPMOD64
#define REL_DTPOFF    R_AARCH64_TLS_DTPREL64
#define REL_TPOFF     R_AARCH64_TLS_TPREL64
#define REL_TLSDESC   R_AARCH64_TLSDESC
#define REL_IRELATIVE R_AARCH64_IRELATIVE

#endif
