/* lib-spfxd — x86-64 feature bits in __cpu_features (also used from
 * assembly, so plain defines only). */
#ifndef _SPFXD_CPU_H
#define _SPFXD_CPU_H

#define CPU_AVX2 1      /* AVX2 instructions and OS-enabled YMM state */
#define CPU_ERMS 2      /* enhanced rep movsb/stosb */
#define CPU_FSRM 4      /* fast short rep movsb */
#define CPU_FMA  8      /* FMA3 and OS-enabled YMM state */

#endif
