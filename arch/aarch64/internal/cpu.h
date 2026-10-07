/* lib-spfxd — AArch64 feature bits in __cpu_features (from AT_HWCAP).
 * Plain defines only (also used from assembly). */
#ifndef _SPFXD_CPU_H
#define _SPFXD_CPU_H

#define CPU_ASIMD   1      /* Advanced SIMD (always present on Linux AArch64) */
#define CPU_ATOMICS 2      /* ARMv8.1 LSE atomics */

#endif
