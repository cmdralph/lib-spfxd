/* lib-spfxd — <sys/sysinfo.h> */
#ifndef _SYS_SYSINFO_H
#define _SYS_SYSINFO_H
#include <features.h>
struct sysinfo {
	long uptime;
	unsigned long loads[3];
	unsigned long totalram, freeram, sharedram, bufferram, totalswap, freeswap;
	unsigned short procs, pad;
	unsigned long totalhigh, freehigh;
	unsigned int mem_unit;
	char __reserved[256];
};
__SPFXD_BEGIN_DECLS
int sysinfo(struct sysinfo *);
int get_nprocs_conf(void);
int get_nprocs(void);
long get_phys_pages(void);
long get_avphys_pages(void);
__SPFXD_END_DECLS
#endif
