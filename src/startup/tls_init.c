/*
 * lib-spfxd — static TLS setup for the initial thread.
 *
 * Reads the executable's PT_TLS program header, computes where its TLS
 * block sits relative to the thread pointer (variant II: below it), builds
 * the initial thread's dtv + TLS + TCB area and installs the thread pointer.
 *
 * The block offset satisfies two constraints: it is at least the segment's
 * memory size, and (thread pointer - offset) is congruent to the segment's
 * virtual address modulo its alignment, so that the linker's statically
 * computed TP-relative offsets are correct even for a segment whose start
 * is not aligned.
 *
 * Small TLS areas use a static buffer; only programs with large TLS need an
 * mmap at startup.
 */
#include <elf.h>
#include <string.h>
#include <sys/mman.h>
#include "libc.h"
#include "pthread_impl.h"

static struct tls_module main_tls;

#define BUILTIN_TLS_EXTRA 2048
static unsigned char builtin_tls[sizeof(struct pthread) + BUILTIN_TLS_EXTRA]
	__attribute__((__aligned__(64)));

extern const size_t _DYNAMIC[] __attribute__((__weak__, __visibility__("hidden")));

hidden void *__copy_tls(unsigned char *mem)
{
	uintptr_t *dtv = (uintptr_t *)mem;
	uintptr_t top = (uintptr_t)mem + __libc.tls_size - sizeof(struct pthread);
	top &= -(uintptr_t)__libc.tls_align;
	struct pthread *td = (struct pthread *)top;
	size_t i = 1;

	for (struct tls_module *p = __libc.tls_head; p; p = p->next, i++) {
		unsigned char *blk = (unsigned char *)top - p->offset;
		dtv[i] = (uintptr_t)blk;
		memcpy(blk, p->image, p->len);
		memset(blk + p->len, 0, p->size - p->len);
	}
	dtv[0] = __libc.tls_cnt;
	td->self = td;
	td->dtv = dtv;
	return td;
}

/* Layout size for the current module set.  Shared with the dynamic linker. */
hidden void __tls_layout_finish(size_t max_offset)
{
	size_t align = __libc.tls_align;
	if (align < 16) align = 16;
	__libc.tls_align = align;
	__libc.tls_size = (__libc.tls_cnt + 1) * sizeof(void *) + max_offset
		+ align + sizeof(struct pthread);
	__libc.tls_size = (__libc.tls_size + 15) & -16UL;
}

hidden void __init_tls(size_t *aux)
{
	const Elf64_Phdr *phdr = (const void *)aux[AT_PHDR], *tls_ph = 0;
	size_t phnum = aux[AT_PHNUM], phent = aux[AT_PHENT];
	size_t base = 0;
	int have_base = 0;
	unsigned char *mem;

	__libc.default_stack = DEFAULT_STACK_SIZE;
	__libc.default_guard = DEFAULT_GUARD_SIZE;
	for (; phnum; phnum--, phdr = (const void *)((const char *)phdr + phent)) {
		if (phdr->p_type == PT_PHDR) {
			base = aux[AT_PHDR] - phdr->p_vaddr;
			have_base = 1;
		} else if (phdr->p_type == PT_DYNAMIC && _DYNAMIC && !have_base) {
			base = (size_t)_DYNAMIC - phdr->p_vaddr;
		} else if (phdr->p_type == PT_TLS) {
			tls_ph = phdr;
		} else if (phdr->p_type == PT_GNU_STACK) {
			size_t s = phdr->p_memsz;
			if (s > __libc.default_stack)
				__libc.default_stack = s < DEFAULT_STACK_MAX ? s : DEFAULT_STACK_MAX;
		}
	}

	__libc.tls_align = 16;
	if (tls_ph && tls_ph->p_memsz) {
		main_tls.image = (const void *)(base + tls_ph->p_vaddr);
		main_tls.len = tls_ph->p_filesz;
		main_tls.size = tls_ph->p_memsz;
		main_tls.align = tls_ph->p_align ? tls_ph->p_align : 1;
		main_tls.offset = main_tls.size +
			((-(uintptr_t)main_tls.image - main_tls.size) & (main_tls.align - 1));
		if (main_tls.align > __libc.tls_align) __libc.tls_align = main_tls.align;
		__libc.tls_head = &main_tls;
		__libc.tls_cnt = 1;
	}
	__tls_layout_finish(main_tls.offset);

	if (__libc.tls_size <= sizeof builtin_tls) {
		mem = builtin_tls;
	} else {
		long r = __syscall(SYS_mmap, 0, __libc.tls_size, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (__is_err(r)) a_crash();
		mem = (unsigned char *)r;
	}

	struct pthread *td = __copy_tls(mem);
	if (__set_thread_area(td) < 0) a_crash();
	td->tid = (int)__syscall(SYS_set_tid_address, &td->tid);
	td->detach_state = DT_JOINABLE;
	td->is_main = 1;
	td->prev = td->next = td;
	td->stack_size = __libc.default_stack;
	__thread_count = 1;
}
