/* lib-spfxd — RISC-V 64 dynamic relocation types, under the names the
 * dynamic linker uses for every architecture.  RISC-V has no GLOB_DAT:
 * GOT entries carry R_RISCV_64 like any absolute word (REL_GOT is
 * therefore not defined). */
#ifndef _SPFXD_RELOC_H
#define _SPFXD_RELOC_H
#include <elf.h>

#define ARCH_NAME     "riscv64"
#define ELF_MACHINE   EM_RISCV
#define REL_NONE      R_RISCV_NONE
#define REL_SYMBOLIC  R_RISCV_64
#define REL_PLT       R_RISCV_JUMP_SLOT
#define REL_RELATIVE  R_RISCV_RELATIVE
#define REL_COPY      R_RISCV_COPY
#define REL_DTPMOD    R_RISCV_TLS_DTPMOD64
#define REL_DTPOFF    R_RISCV_TLS_DTPREL64
#define REL_TPOFF     R_RISCV_TLS_TPREL64
#define REL_IRELATIVE R_RISCV_IRELATIVE

#endif
