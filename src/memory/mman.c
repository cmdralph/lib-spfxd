/* lib-spfxd — memory mapping wrappers and brk/sbrk. */
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include "syscall.h"

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	if (off & 4095) {
		errno = EINVAL;
		return MAP_FAILED;
	}
	if (len >= PTRDIFF_MAX) {
		errno = ENOMEM;
		return MAP_FAILED;
	}
	return (void *)__sysret(SYS_mmap, addr, len, prot, flags, fd, off);
}
weak_alias(mmap, mmap64);

int munmap(void *addr, size_t len) { return (int)__sysret(SYS_munmap, addr, len); }
int mprotect(void *addr, size_t len, int prot)
{
	/* POSIX lets addr be unaligned; round the range out to whole pages */
	uintptr_t s = (uintptr_t)addr & -4096UL, e = ((uintptr_t)addr + len + 4095) & -4096UL;
	return (int)__sysret(SYS_mprotect, s, e - s, prot);
}
int msync(void *addr, size_t len, int flags) { return (int)__sysret_cp(SYS_msync, addr, len, flags); }
int madvise(void *addr, size_t len, int advice) { return (int)__sysret(SYS_madvise, addr, len, advice); }
int posix_madvise(void *addr, size_t len, int advice)
{
	if (advice == POSIX_MADV_DONTNEED) return 0;   /* non-destructive semantics */
	return (int)-__syscall(SYS_madvise, addr, len, advice);
}
int mincore(void *addr, size_t len, unsigned char *vec) { return (int)__sysret(SYS_mincore, addr, len, vec); }
int mlock(const void *addr, size_t len) { return (int)__sysret(SYS_mlock, addr, len); }
int munlock(const void *addr, size_t len) { return (int)__sysret(SYS_munlock, addr, len); }
int mlockall(int flags) { return (int)__sysret(SYS_mlockall, flags); }
int munlockall(void) { return (int)__sysret(SYS_munlockall); }
int mlock2(const void *addr, size_t len, unsigned flags) { return (int)__sysret(SYS_mlock2, addr, len, flags); }
int memfd_create(const char *name, unsigned flags) { return (int)__sysret(SYS_memfd_create, name, flags); }

void *mremap(void *old, size_t oldlen, size_t newlen, int flags, ...)
{
	void *new = 0;
	if (flags & MREMAP_FIXED) {
		va_list ap;
		va_start(ap, flags);
		new = va_arg(ap, void *);
		va_end(ap);
	}
	return (void *)__sysret(SYS_mremap, old, oldlen, newlen, flags, new);
}

/*
 * brk/sbrk operate on the kernel program break.  The allocator never uses
 * the break, so programs that manage it themselves cannot conflict with
 * malloc.
 */
static uintptr_t cur_brk;

int brk(void *end)
{
	uintptr_t r = (uintptr_t)__syscall(SYS_brk, end);
	if (r != (uintptr_t)end) {
		errno = ENOMEM;
		return -1;
	}
	cur_brk = r;
	return 0;
}

void *sbrk(intptr_t inc)
{
	if (!cur_brk) cur_brk = (uintptr_t)__syscall(SYS_brk, 0);
	if (!inc) return (void *)cur_brk;
	uintptr_t old = cur_brk, want = old + (uintptr_t)inc;
	if ((inc > 0 && want < old) || (inc < 0 && want > old)) {
		errno = ENOMEM;
		return (void *)-1;
	}
	uintptr_t r = (uintptr_t)__syscall(SYS_brk, want);
	if (r != want) {
		errno = ENOMEM;
		return (void *)-1;
	}
	cur_brk = r;
	return (void *)old;
}

