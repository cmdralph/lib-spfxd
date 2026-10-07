/*
 * lib-spfxd — x86-64 CPU feature detection.
 *
 * __cpu_features is read by the assembly string routines (and anything
 * else with an optional fast path) to choose an implementation at run
 * time.  It starts out zero, which selects the SSE2 baseline every x86-64
 * CPU has, so code that runs before __init_cpu (the dynamic linker's own
 * relocation) is still correct.
 *
 * AVX2 is only reported when the OS saves the YMM state (OSXSAVE and
 * XCR0 bits 1-2): a CPU that supports the instructions under a kernel that
 * does not context-switch the upper halves must not use them.
 */
#include "libc.h"
#include "cpu.h"

hidden unsigned __cpu_features;

static void cpuid(unsigned leaf, unsigned sub, unsigned r[4])
{
	__asm__ volatile ("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3])
		: "a"(leaf), "c"(sub));
}

hidden void __init_cpu(void)
{
	unsigned r[4], max, f = 0;
	cpuid(0, 0, r);
	max = r[0];
	if (max < 1) return;
	cpuid(1, 0, r);
	int osxsave = (r[2] >> 27) & 1, avx = (r[2] >> 28) & 1, fma = (r[2] >> 12) & 1;
	int ymm_ok = 0;
	if (osxsave && avx) {
		unsigned lo, hi;
		__asm__ volatile ("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
		ymm_ok = (lo & 6) == 6;
	}
	if (ymm_ok && fma) f |= CPU_FMA;
	if (max >= 7) {
		cpuid(7, 0, r);
		if (ymm_ok && ((r[1] >> 5) & 1)) f |= CPU_AVX2;
		if ((r[1] >> 9) & 1) f |= CPU_ERMS;
		if ((r[3] >> 4) & 1) f |= CPU_FSRM;
	}
	__cpu_features = f;
}
