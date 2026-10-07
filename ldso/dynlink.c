/*
 * lib-spfxd — the dynamic linker (stages 2 and 3) and the dl* interface.
 *
 * Startup (the kernel runs libc.so as the program interpreter):
 *   stage 1  dlstart.c relocates libc.so against itself
 *   stage 2  __dls2 records libc.so's own module description
 *   stage 3  __dls3 initializes the C runtime (environment, auxiliary
 *            vector, a provisional thread pointer so errno and malloc
 *            work), describes or maps the main program, loads LD_PRELOAD
 *            and all DT_NEEDED dependencies breadth first, lays out static
 *            TLS for every module, applies all relocations (dependencies
 *            before dependents, the program last so its copy relocations
 *            see relocated data), write-protects RELRO, installs the final
 *            TLS area, runs constructors in dependency order and jumps to
 *            the program's entry point.
 *
 * Symbol resolution is immediate (no lazy binding), uses the GNU hash
 * table when present and the SysV one otherwise, and searches the global
 * scope in load order (program, preloads, dependencies) followed by the
 * requesting library's own dependency closure for RTLD_LOCAL objects.
 * Symbol versions are not matched; hidden (non-default) versioned
 * definitions are skipped.
 *
 * TLS: modules present at startup get static TLS (variant II, below the
 * thread pointer).  Modules loaded by dlopen get dynamic TLS: a block is
 * allocated for a thread on its first __tls_get_addr for that module.
 * Such modules may not use the initial-exec model (REL_TPOFF).
 *
 * dlclose never unmaps: objects stay loaded until exit (their destructors
 * run at exit).  TLS descriptors (REL_TLSDESC) are not supported.
 */
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <link.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "libc.h"
#include "reloc.h"
#include "pthread_impl.h"
#include "syscall.h"

#define DYN_CNT 38
#define DT_RELRSZ_ 35
#define DT_RELR_ 36

struct dso {
	/* struct link_map prefix, for debuggers */
	unsigned char *base;
	char *name;
	Elf64_Dyn *dynv;
	struct dso *next, *prev;

	const Elf64_Phdr *phdr;
	size_t phnum, phentsize;
	const Elf64_Sym *syms;
	const uint32_t *hashtab;
	const uint32_t *ghashtab;
	const uint16_t *versym;
	const char *strings;
	size_t dyn[DYN_CNT];
	unsigned char *map;
	size_t map_len;
	dev_t dev;
	ino_t ino;
	const char *shortname;
	const char *soname;
	char *rpath_orig, *runpath_orig;
	char *origin;                    /* directory, for $ORIGIN */
	unsigned char relocated, constructed, global, kernel_mapped, mark, dyn_tls;
	int refcnt;
	struct dso **deps;               /* direct DT_NEEDED dependencies */
	size_t ndeps;
	struct dso **closure;            /* self + all dependencies, BFS order */
	size_t nclosure;
	struct tls_module tls;
	size_t tls_id;
	size_t relro_start, relro_end;
	struct dso *fini_next;
	size_t init_array_sz, fini_array_sz;
	size_t preinit_array, preinit_array_sz;
};

static struct dso ldso;                  /* libc.so itself */
static struct dso *head, *tail;          /* all loaded objects, load order */
static struct dso *fini_head;
static int runtime;                      /* startup finished: errors are recoverable */
static char *env_path, *sys_path;
static size_t tls_static_offset;         /* end of the static TLS area */
static size_t tls_next_id;
static struct tls_module **tls_mods;     /* index = module id */
static size_t tls_mods_cap;
static size_t dl_adds;
static int ldd_mode;
static char **app_argv;
static int app_argc;
static pthread_mutex_t dl_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

struct r_debug _r_debug;
void _dl_debug_state(void);
hidden void __dl_seterr(const char *, ...);
hidden _Noreturn void __dls2(unsigned char *, size_t *);
hidden _Noreturn void __dls3(size_t *);

/* Debuggers set a breakpoint here (r_brk) to observe library changes. */
__attribute__((__noinline__)) void _dl_debug_state(void)
{
	__asm__ __volatile__("" ::: "memory");
}

/* ------------------------------------------------------------------ errors */

static _Noreturn void startup_fail(const char *msg)
{
	struct { const void *b; size_t l; } iov[3] = {
		{ "lib-spfxd: ", 11 }, { msg, strlen(msg) }, { "\n", 1 } };
	__syscall(SYS_writev, 2, iov, 3);
	__syscall(SYS_exit_group, 127);
	for (;;);
}

/* Report an error: fatal during startup, recorded for dlerror() after. */
static void error(const char *fmt, ...)
{
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (!runtime) startup_fail(buf);
	__dl_seterr("%s", buf);
}

/* ---------------------------------------------------------- dyn parsing */

static void decode_vec(const Elf64_Dyn *v, size_t *a, size_t cnt)
{
	for (size_t i = 0; i < cnt; i++) a[i] = 0;
	for (; v->d_tag; v++)
		if ((size_t)v->d_tag < cnt) {
			a[v->d_tag] = v->d_un.d_val;
			if (v->d_tag < 32) a[0] |= 1UL << v->d_tag;
		}
}

static size_t dyn_lookup(const Elf64_Dyn *v, Elf64_Sxword tag)
{
	for (; v->d_tag; v++)
		if (v->d_tag == tag) return v->d_un.d_val;
	return 0;
}

static void decode_dyn(struct dso *p)
{
	decode_vec(p->dynv, p->dyn, DYN_CNT);
	p->syms = (const void *)(p->base + p->dyn[DT_SYMTAB]);
	p->strings = (const void *)(p->base + p->dyn[DT_STRTAB]);
	if (p->dyn[DT_HASH]) p->hashtab = (const void *)(p->base + p->dyn[DT_HASH]);
	size_t g = dyn_lookup(p->dynv, DT_GNU_HASH);
	if (g) p->ghashtab = (const void *)(p->base + g);
	size_t vs = dyn_lookup(p->dynv, DT_VERSYM);
	if (vs) p->versym = (const void *)(p->base + vs);
	if (p->dyn[DT_RPATH]) p->rpath_orig = (char *)p->strings + p->dyn[DT_RPATH];
	if (p->dyn[DT_RUNPATH]) p->runpath_orig = (char *)p->strings + p->dyn[DT_RUNPATH];
	if (p->dyn[DT_SONAME]) p->soname = p->strings + p->dyn[DT_SONAME];
	p->init_array_sz = p->dyn[DT_INIT_ARRAYSZ];
	p->fini_array_sz = p->dyn[DT_FINI_ARRAYSZ];
	p->preinit_array = dyn_lookup(p->dynv, DT_PREINIT_ARRAY);
	p->preinit_array_sz = dyn_lookup(p->dynv, DT_PREINIT_ARRAYSZ);
}

/* Scan program headers: dynamic section, TLS, RELRO. */
static void scan_phdrs(struct dso *p)
{
	const Elf64_Phdr *ph = p->phdr;
	for (size_t i = 0; i < p->phnum; i++, ph = (const void *)((const char *)ph + p->phentsize)) {
		if (ph->p_type == PT_DYNAMIC) {
			p->dynv = (void *)(p->base + ph->p_vaddr);
		} else if (ph->p_type == PT_GNU_RELRO) {
			p->relro_start = ph->p_vaddr & -(size_t)__libc.page_size;
			p->relro_end = (ph->p_vaddr + ph->p_memsz) & -(size_t)__libc.page_size;
		} else if (ph->p_type == PT_TLS && ph->p_memsz) {
			p->tls.image = p->base + ph->p_vaddr;
			p->tls.len = ph->p_filesz;
			p->tls.size = ph->p_memsz;
			p->tls.align = ph->p_align ? ph->p_align : 1;
		}
	}
}

/* ------------------------------------------------------------ symbols */

static uint32_t sysv_hash(const char *s0)
{
	const unsigned char *s = (const void *)s0;
	uint32_t h = 0;
	while (*s) {
		h = 16 * h + *s++;
		h ^= h >> 24 & 0xf0;
	}
	return h & 0xfffffff;
}

static uint32_t gnu_hash(const char *s0)
{
	const unsigned char *s = (const void *)s0;
	uint32_t h = 5381;
	for (; *s; s++) h += h * 32 + *s;
	return h;
}

#define OK_TYPES (1 << STT_NOTYPE | 1 << STT_OBJECT | 1 << STT_FUNC | 1 << STT_COMMON | \
                  1 << STT_TLS | 1 << STT_GNU_IFUNC)
#define OK_BINDS (1 << STB_GLOBAL | 1 << STB_WEAK | 1 << STB_GNU_UNIQUE)

static int sym_ok(const struct dso *d, const Elf64_Sym *s, size_t i)
{
	if (!(1 << (s->st_info & 0xf) & OK_TYPES)) return 0;
	if (!(1 << (s->st_info >> 4) & OK_BINDS)) return 0;
	if (s->st_shndx == SHN_UNDEF) return 0;
	if (!s->st_value && (s->st_info & 0xf) != STT_TLS) return 0;
	if (d->versym && (d->versym[i] & 0x8000)) return 0;     /* hidden version */
	return 1;
}

static const Elf64_Sym *gnu_lookup(const struct dso *d, const char *s, uint32_t h)
{
	const uint32_t *gh = d->ghashtab;
	uint32_t nbuckets = gh[0], symoff = gh[1], bloom_size = gh[2], shift = gh[3];
	const uint64_t *bloom = (const void *)(gh + 4);
	const uint32_t *buckets = (const void *)(bloom + bloom_size);
	const uint32_t *chain = buckets + nbuckets;
	uint64_t w = bloom[(h / 64) & (bloom_size - 1)];
	uint64_t mask = (1ULL << (h % 64)) | (1ULL << ((h >> shift) % 64));
	if ((w & mask) != mask) return 0;
	uint32_t i = buckets[h % nbuckets];
	if (i < symoff) return 0;
	for (;; i++) {
		uint32_t h2 = chain[i - symoff];
		if ((h | 1) == (h2 | 1)) {
			const Elf64_Sym *sym = d->syms + i;
			if (!strcmp(s, d->strings + sym->st_name) && sym_ok(d, sym, i)) return sym;
		}
		if (h2 & 1) return 0;
	}
}

static const Elf64_Sym *sysv_lookup(const struct dso *d, const char *s, uint32_t h)
{
	const uint32_t *ht = d->hashtab;
	uint32_t nbucket = ht[0];
	const uint32_t *bucket = ht + 2, *chain = bucket + nbucket;
	for (uint32_t i = bucket[h % nbucket]; i; i = chain[i]) {
		const Elf64_Sym *sym = d->syms + i;
		if (!strcmp(s, d->strings + sym->st_name) && sym_ok(d, sym, i)) return sym;
	}
	return 0;
}

struct symdef {
	const Elf64_Sym *sym;
	struct dso *dso;
};

static const Elf64_Sym *lookup_in(const struct dso *d, const char *s, uint32_t gh, uint32_t *sh, int *have_sh)
{
	if (d->ghashtab) return gnu_lookup(d, s, gh);
	if (!d->hashtab) return 0;
	if (!*have_sh) { *sh = sysv_hash(s); *have_sh = 1; }
	return sysv_lookup(d, s, *sh);
}

/* Global scope (in load order), then `extra` (a dependency closure). */
static struct symdef find_sym(const char *s, struct dso **extra, size_t nextra, struct dso *skip)
{
	uint32_t gh = gnu_hash(s), sh = 0;
	int have_sh = 0;
	struct symdef def = { 0, 0 };
	for (struct dso *d = head; d; d = d->next) {
		if (!d->global || d == skip) continue;
		const Elf64_Sym *sym = lookup_in(d, s, gh, &sh, &have_sh);
		if (sym) return (struct symdef){ sym, d };
	}
	for (size_t i = 0; i < nextra; i++) {
		if (extra[i] == skip) continue;
		const Elf64_Sym *sym = lookup_in(extra[i], s, gh, &sh, &have_sh);
		if (sym) return (struct symdef){ sym, extra[i] };
	}
	return def;
}

/* Search only within a list (dlsym on a handle). */
static struct symdef find_sym_list(const char *s, struct dso **list, size_t n)
{
	uint32_t gh = gnu_hash(s), sh = 0;
	int have_sh = 0;
	for (size_t i = 0; i < n; i++) {
		const Elf64_Sym *sym = lookup_in(list[i], s, gh, &sh, &have_sh);
		if (sym) return (struct symdef){ sym, list[i] };
	}
	return (struct symdef){ 0, 0 };
}

/* -------------------------------------------------------- relocations */

/* Copy relocations move an object into the program; every other
 * reference to the original address (including through aliases such as
 * environ/__environ) must follow it. */
struct moved { size_t from, to, size; };
static struct moved *moved;
static size_t nmoved;

static int do_relocs(struct dso *dso, const Elf64_Rela *rel, size_t size)
{
	unsigned char *base = dso->base;
	size_t n = size / sizeof *rel;
	for (size_t i = 0; i < n; i++, rel++) {
		uint32_t type = (uint32_t)ELF64_R_TYPE(rel->r_info);
		uint32_t si = (uint32_t)ELF64_R_SYM(rel->r_info);
		size_t *where = (size_t *)(base + rel->r_offset);
		size_t addend = (size_t)rel->r_addend;
		if (type == REL_NONE) continue;
		if (type == REL_RELATIVE) {
			*where = (size_t)base + addend;
			continue;
		}
		if (type == REL_IRELATIVE) {
			*where = ((size_t (*)(void))(base + addend))();
			continue;
		}
		const Elf64_Sym *sym = si ? dso->syms + si : 0;
		struct symdef def = { 0, 0 };
		if (sym) {
			const char *name = dso->strings + sym->st_name;
			if (ELF64_ST_BIND(sym->st_info) == STB_LOCAL) {
				def = (struct symdef){ sym, dso };
			} else {
				def = find_sym(name, dso->closure, dso->nclosure,
				               type == REL_COPY ? dso : 0);
				if (!def.sym && ELF64_ST_BIND(sym->st_info) != STB_WEAK) {
					error("%s: symbol not found: %s", dso->name, name);
					return -1;
				}
			}
		} else {
			def.dso = dso;               /* module-relative TLS relocations */
		}
		size_t sv = 0;
		if (def.sym) {
			sv = (size_t)def.dso->base + def.sym->st_value;
			if (ELF64_ST_TYPE(def.sym->st_info) == STT_GNU_IFUNC && type != REL_COPY)
				sv = ((size_t (*)(void))sv)();
		}
		switch (type) {
		case REL_SYMBOLIC:
			*where = sv + addend;
			break;
		case REL_GOT:
		case REL_PLT:
			*where = sv;
			break;
#ifdef REL_PC32
		case REL_PC32:
			*(uint32_t *)where = (uint32_t)(sv + addend - (size_t)where);
			break;
		case REL_32:
		case REL_32S:
			*(uint32_t *)where = (uint32_t)(sv + addend);
			break;
		case REL_SIZE64:
			*where = (def.sym ? def.sym->st_size : 0) + addend;
			break;
#endif
		case REL_COPY:
			if (def.sym) {
				memcpy(where, (void *)sv, sym->st_size);
				struct moved *nm = realloc(moved, (nmoved + 1) * sizeof *nm);
				if (nm) {
					moved = nm;
					moved[nmoved++] = (struct moved){ sv, (size_t)where, sym->st_size };
				}
			}
			break;
		case REL_DTPMOD:
			*where = def.dso ? def.dso->tls_id : 0;
			break;
		case REL_DTPOFF:
			*where = (def.sym ? def.sym->st_value : 0) + addend;
			break;
		case REL_TPOFF:
			if (def.dso && def.dso->dyn_tls) {
				error("%s: cannot use the initial-exec TLS model in a dynamically loaded library",
				      dso->name);
				return -1;
			}
#if TLS_ABOVE_TP
			*where = (def.sym ? def.sym->st_value : 0) + addend + (def.dso ? def.dso->tls.offset : 0);
#else
			*where = (def.sym ? def.sym->st_value : 0) + addend - (def.dso ? def.dso->tls.offset : 0);
#endif
			break;
		default:
			error("%s: unsupported relocation type %u", dso->name, type);
			return -1;
		}
	}
	return 0;
}

static void do_relr(struct dso *dso)
{
	size_t relr = dso->dyn[DT_RELR_], sz = dso->dyn[DT_RELRSZ_];
	if (!relr || dso == &ldso) return;      /* libc.so's were applied in stage 1 */
	const size_t *e = (const void *)(dso->base + relr);
	size_t *where = 0, b = (size_t)dso->base;
	for (size_t i = 0; i < sz / sizeof *e; i++) {
		if (!(e[i] & 1)) {
			where = (size_t *)(b + e[i]);
			*where++ += b;
		} else {
			size_t bits = e[i] >> 1;
			for (size_t k = 0; bits; k++, bits >>= 1)
				if (bits & 1) where[k] += b;
			where += 63;
		}
	}
}

static int reloc_dso(struct dso *p)
{
	if (p->relocated) return 0;
	if (p != &ldso) do_relr(p);
	/* libc.so's RELATIVE relocations were applied in stage 1 and its RELR
	 * data must not be adjusted twice; re-running RELA is idempotent */
	if (p->dyn[DT_RELA] && do_relocs(p, (const void *)(p->base + p->dyn[DT_RELA]), p->dyn[DT_RELASZ]))
		return -1;
	if (p->dyn[DT_JMPREL] && do_relocs(p, (const void *)(p->base + p->dyn[DT_JMPREL]), p->dyn[DT_PLTRELSZ]))
		return -1;
	p->relocated = 1;
	return 0;
}

static int protect_relro(struct dso *p)
{
	if (p->relro_end > p->relro_start &&
	    __syscall(SYS_mprotect, p->base + p->relro_start, p->relro_end - p->relro_start, PROT_READ) < 0) {
		error("%s: cannot apply RELRO protection", p->name);
		return -1;
	}
	return 0;
}

/* Rewrites already-applied pointer relocations of `p` that still point
 * into an object moved by a copy relocation (see struct moved). */

static void redirect_moved(struct dso *p)
{
	if (!nmoved) return;
	for (int pass = 0; pass < 2; pass++) {
		size_t off = pass ? p->dyn[DT_JMPREL] : p->dyn[DT_RELA];
		size_t sz = pass ? p->dyn[DT_PLTRELSZ] : p->dyn[DT_RELASZ];
		if (!off) continue;
		const Elf64_Rela *r = (const void *)(p->base + off);
		for (size_t i = 0; i < sz / sizeof *r; i++, r++) {
			uint32_t type = (uint32_t)ELF64_R_TYPE(r->r_info);
			if (type != REL_GOT && type != REL_SYMBOLIC) continue;
			size_t *where = (size_t *)(p->base + r->r_offset);
			for (size_t k = 0; k < nmoved; k++)
				if (*where - moved[k].from < moved[k].size || *where == moved[k].from)
					*where = *where - moved[k].from + moved[k].to;
		}
	}
}

/* --------------------------------------------------------- mapping */

static int prot_of(uint32_t flags)
{
	return (flags & PF_R ? PROT_READ : 0) | (flags & PF_W ? PROT_WRITE : 0) |
	       (flags & PF_X ? PROT_EXEC : 0);
}

/* Map an ELF object from fd into p (base, map, phdr...). Returns 0 or -1. */
static int map_library(int fd, struct dso *p, int allow_exec)
{
	union {
		Elf64_Ehdr eh;
		unsigned char buf[1024];
	} u;
	ssize_t l = pread(fd, u.buf, sizeof u.buf, 0);
	if (l < (ssize_t)sizeof u.eh) return -1;
	Elf64_Ehdr *eh = &u.eh;
	if (memcmp(eh->e_ident, ELFMAG, SELFMAG) || eh->e_ident[EI_CLASS] != ELFCLASS64 ||
	    eh->e_ident[EI_DATA] != ELFDATA2LSB || eh->e_machine != EM_X86_64 ||
	    (eh->e_type != ET_DYN && !(allow_exec && eh->e_type == ET_EXEC)) ||
	    eh->e_phentsize < sizeof(Elf64_Phdr)) {
		errno = ENOEXEC;
		return -1;
	}
	size_t phsize = (size_t)eh->e_phnum * eh->e_phentsize;
	Elf64_Phdr *ph0;
	unsigned char *phbuf = 0;
	if (eh->e_phoff + phsize <= (size_t)l) {
		ph0 = (void *)(u.buf + eh->e_phoff);
	} else {
		phbuf = malloc(phsize);
		if (!phbuf || pread(fd, phbuf, phsize, (off_t)eh->e_phoff) != (ssize_t)phsize) {
			free(phbuf);
			errno = ENOEXEC;
			return -1;
		}
		ph0 = (void *)phbuf;
	}
	size_t page = __libc.page_size;
	size_t minv = SIZE_MAX, maxv = 0, phdr_vaddr = 0;
	int have_phdr_seg = 0;
	const Elf64_Phdr *ph = ph0;
	for (size_t i = 0; i < eh->e_phnum; i++, ph = (const void *)((const char *)ph + eh->e_phentsize)) {
		if (ph->p_type != PT_LOAD) continue;
		if (ph->p_vaddr < minv) minv = ph->p_vaddr;
		if (ph->p_vaddr + ph->p_memsz > maxv) maxv = ph->p_vaddr + ph->p_memsz;
		/* the program headers themselves, if inside a loaded segment */
		if (eh->e_phoff >= ph->p_offset && eh->e_phoff + phsize <= ph->p_offset + ph->p_filesz) {
			phdr_vaddr = ph->p_vaddr + (eh->e_phoff - ph->p_offset);
			have_phdr_seg = 1;
		}
	}
	if (minv == SIZE_MAX) goto noexec;
	minv &= -page;
	maxv = (maxv + page - 1) & -page;
	size_t span = maxv - minv;
	unsigned char *map, *base;
	if (eh->e_type == ET_DYN) {
		map = mmap(0, span, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (map == MAP_FAILED) goto fail;
		base = map - minv;
	} else {
		map = (unsigned char *)minv;
		base = 0;
	}
	ph = ph0;
	for (size_t i = 0; i < eh->e_phnum; i++, ph = (const void *)((const char *)ph + eh->e_phentsize)) {
		if (ph->p_type != PT_LOAD || !ph->p_memsz) continue;
		int prot = prot_of(ph->p_flags);
		size_t this_min = ph->p_vaddr & -page;
		size_t file_end = ph->p_vaddr + ph->p_filesz;
		size_t this_max = (file_end + page - 1) & -page;
		size_t off = ph->p_offset & -page;
		if (this_max > this_min &&
		    mmap(base + this_min, this_max - this_min, prot | (ph->p_memsz > ph->p_filesz ? PROT_WRITE : 0),
		         MAP_PRIVATE | MAP_FIXED, fd, (off_t)off) == MAP_FAILED)
			goto unmap;
		if (ph->p_memsz > ph->p_filesz) {
			/* .bss: clear the partial page, map anonymous zero pages for
			 * the rest */
			size_t zstart = file_end, zpage = (zstart + page - 1) & -page;
			size_t zend = (ph->p_vaddr + ph->p_memsz + page - 1) & -page;
			if (zpage > zstart && this_max > this_min) memset(base + zstart, 0, zpage - zstart);
			if (zend > zpage &&
			    mmap(base + zpage, zend - zpage, prot, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
				goto unmap;
			if (!(prot & PROT_WRITE) && this_max > this_min)
				mprotect(base + this_min, this_max - this_min, prot);
		}
	}
	p->map = map;
	p->map_len = span;
	p->base = base;
	if (have_phdr_seg) {
		p->phdr = (const void *)(base + phdr_vaddr);
	} else {
		/* keep a private copy */
		Elf64_Phdr *copy = malloc(phsize);
		if (!copy) goto unmap;
		memcpy(copy, ph0, phsize);
		p->phdr = copy;
	}
	p->phnum = eh->e_phnum;
	p->phentsize = eh->e_phentsize;
	free(phbuf);
	return 0;
unmap:
	if (eh->e_type == ET_DYN) munmap(map, span);
fail:
	free(phbuf);
	return -1;
noexec:
	free(phbuf);
	errno = ENOEXEC;
	return -1;
}

/* ---------------------------------------------------------- loading */

static struct dso *alloc_dso(const char *name)
{
	size_t l = strlen(name);
	struct dso *p = calloc(1, sizeof *p + l + 1);
	if (!p) return 0;
	p->name = (char *)(p + 1);
	memcpy(p->name, name, l + 1);
	const char *s = strrchr(p->name, '/');
	p->shortname = s ? s + 1 : p->name;
	p->refcnt = 1;
	return p;
}

static void set_origin(struct dso *p)
{
	const char *s = strrchr(p->name, '/');
	if (!s) {
		p->origin = strdup(".");
	} else {
		size_t n = (size_t)(s - p->name);
		p->origin = malloc(n + 2);
		if (p->origin) {
			memcpy(p->origin, p->name, n ? n : 1);
			p->origin[n ? n : 1] = 0;
		}
	}
}

/* Expand $ORIGIN / ${ORIGIN} in a search path. */
static char *expand_origin(const char *path, struct dso *p)
{
	if (!strchr(path, '$')) return strdup(path);
	if (__libc.secure) return 0;
	if (!p->origin) set_origin(p);
	if (!p->origin) return 0;
	size_t ol = strlen(p->origin), n = strlen(path) + 1;
	for (const char *s = path; (s = strchr(s, '$')); s++) n += ol;
	char *out = malloc(n), *o = out;
	if (!out) return 0;
	for (const char *s = path; *s;) {
		if (!strncmp(s, "$ORIGIN", 7) || !strncmp(s, "${ORIGIN}", 9)) {
			memcpy(o, p->origin, ol);
			o += ol;
			s += s[1] == '{' ? 9 : 7;
		} else {
			*o++ = *s++;
		}
	}
	*o = 0;
	return out;
}

/* Try each directory of a colon-separated list. */
static int path_open(const char *name, const char *list, char *buf, size_t bufsz)
{
	if (!list) return -1;
	size_t nl = strlen(name);
	for (const char *s = list; *s;) {
		const char *e = s + strcspn(s, ":\n");
		size_t dl = (size_t)(e - s);
		if (dl + 1 + nl + 1 <= bufsz) {
			if (dl) {
				memcpy(buf, s, dl);
				buf[dl] = '/';
				memcpy(buf + dl + 1, name, nl + 1);
			} else {
				memcpy(buf, name, nl + 1);
			}
			int fd = open(buf, O_RDONLY | O_CLOEXEC);
			if (fd >= 0) return fd;
			if (errno != ENOENT && errno != ENOTDIR && errno != EACCES) return -2;
		}
		s = *e ? e + 1 : e;
	}
	return -1;
}

static int is_libc_name(const char *s)
{
	static const char *const names[] = {
		"libc.so", "libc.so.6", "libm.so", "libm.so.6", "libpthread.so", "libpthread.so.0",
		"libdl.so", "libdl.so.2", "librt.so", "librt.so.1", "libutil.so", "libutil.so.1",
		"libxnet.so", "libresolv.so", "libresolv.so.2", "libcrypt.so", "ld-linux-x86-64.so.2",
	};
	for (size_t i = 0; i < ARRAY_SIZE(names); i++)
		if (!strcmp(s, names[i])) return 1;
	return 0;
}

static void append(struct dso *p)
{
	p->prev = tail;
	if (tail) tail->next = p;
	tail = p;
	if (!head) head = p;
}

static struct dso *load_library(const char *name, struct dso *needed_by);

/* Look up an already loaded object by name. */
static struct dso *find_loaded(const char *name)
{
	for (struct dso *p = head; p; p = p->next) {
		if (!strcmp(p->name, name)) return p;
		if (!strchr(name, '/') && ((p->soname && !strcmp(p->soname, name)) || !strcmp(p->shortname, name)))
			return p;
	}
	return 0;
}

static struct dso *load_library(const char *name, struct dso *needed_by)
{
	char buf[PATH_MAX + 1];
	int fd;
	if (!*name) {
		errno = EINVAL;
		return 0;
	}
	if (!strchr(name, '/')) {
		if (is_libc_name(name)) return &ldso;
		struct dso *p = find_loaded(name);
		if (p) return p;
		fd = -1;
		/* DT_RPATH (only without DT_RUNPATH), LD_LIBRARY_PATH, DT_RUNPATH, system */
		for (struct dso *q = needed_by; fd == -1 && q; q = q == head ? 0 : head) {
			if (q->rpath_orig && !q->runpath_orig) {
				char *rp = expand_origin(q->rpath_orig, q);
				fd = path_open(name, rp, buf, sizeof buf);
				free(rp);
			}
		}
		if (fd == -1) fd = path_open(name, env_path, buf, sizeof buf);
		if (fd == -1 && needed_by && needed_by->runpath_orig) {
			char *rp = expand_origin(needed_by->runpath_orig, needed_by);
			fd = path_open(name, rp, buf, sizeof buf);
			free(rp);
		}
		if (fd == -1) fd = path_open(name, sys_path, buf, sizeof buf);
		if (fd < 0) {
			errno = ENOENT;
			return 0;
		}
		name = buf;
	} else {
		struct dso *p = find_loaded(name);
		if (p) return p;
		fd = open(name, O_RDONLY | O_CLOEXEC);
		if (fd < 0) return 0;
	}
	struct stat st;
	if (fstat(fd, &st) < 0) {
		close(fd);
		return 0;
	}
	for (struct dso *p = head; p; p = p->next) {
		if (p->dev == st.st_dev && p->ino == st.st_ino && p->dev) {
			close(fd);
			return p;
		}
	}
	struct dso *p = alloc_dso(name);
	if (!p) {
		close(fd);
		errno = ENOMEM;
		return 0;
	}
	if (map_library(fd, p, 0) < 0) {
		int e = errno;
		close(fd);
		free(p);
		errno = e;
		return 0;
	}
	close(fd);
	p->dev = st.st_dev;
	p->ino = st.st_ino;
	scan_phdrs(p);
	if (!p->dynv) {
		munmap(p->map, p->map_len);
		free(p);
		errno = ENOEXEC;
		return 0;
	}
	decode_dyn(p);
	if (p->soname && is_libc_name(p->soname) && strcmp(p->soname, "libc.so")) {
		/* another C library (e.g. glibc's libc.so.6) requested by path */
		munmap(p->map, p->map_len);
		free(p);
		return &ldso;
	}
	append(p);
	return p;
}

/* Load the DT_NEEDED entries of p and of everything loaded after it. */
static int load_deps(struct dso *p)
{
	for (; p; p = p->next) {
		if (p->deps) continue;
		size_t cnt = 0;
		for (const Elf64_Dyn *v = p->dynv; v->d_tag; v++)
			if (v->d_tag == DT_NEEDED) cnt++;
		p->deps = calloc(cnt + 1, sizeof *p->deps);
		if (!p->deps) {
			error("out of memory");
			return -1;
		}
		for (const Elf64_Dyn *v = p->dynv; v->d_tag; v++) {
			if (v->d_tag != DT_NEEDED) continue;
			const char *n = p->strings + v->d_un.d_val;
			struct dso *d = load_library(n, p);
			if (!d) {
				error("%s: cannot load %s (needed by %s)", n, strerror(errno), p->name);
				return -1;
			}
			p->deps[p->ndeps++] = d;
		}
	}
	return 0;
}

/* Breadth-first dependency closure of p (including p). */
static int make_closure(struct dso *p)
{
	if (p->closure) return 0;
	size_t cap = 8, n = 0;
	struct dso **q = malloc(cap * sizeof *q);
	if (!q) return -1;
	for (struct dso *d = head; d; d = d->next) d->mark = 0;
	ldso.mark = 0;
	q[n++] = p;
	p->mark = 1;
	for (size_t i = 0; i < n; i++) {
		struct dso *d = q[i];
		for (size_t j = 0; j < d->ndeps; j++) {
			struct dso *e = d->deps[j];
			if (e->mark) continue;
			if (n == cap) {
				struct dso **nq = realloc(q, 2 * cap * sizeof *q);
				if (!nq) { free(q); return -1; }
				q = nq;
				cap *= 2;
			}
			e->mark = 1;
			q[n++] = e;
		}
	}
	p->closure = q;
	p->nclosure = n;
	return 0;
}

/* ------------------------------------------------------------- TLS */

static int tls_register(struct dso *p, int dynamic)
{
	if (!p->tls.size || p->tls_id) return 0;
	size_t id = ++tls_next_id;
	if (id >= tls_mods_cap) {
		size_t nc = tls_mods_cap ? 2 * tls_mods_cap : 16;
		struct tls_module **nm = realloc(tls_mods, nc * sizeof *nm);
		if (!nm) {
			tls_next_id--;
			return -1;
		}
		memset(nm + tls_mods_cap, 0, (nc - tls_mods_cap) * sizeof *nm);
		tls_mods = nm;
		tls_mods_cap = nc;
	}
	tls_mods[id] = &p->tls;
	p->tls_id = id;
	if (!dynamic) {
#if TLS_ABOVE_TP
		/* static: place after the previous module (the first after the
		 * TCB gap), congruent to the image modulo its alignment */
		size_t o = tls_static_offset ? tls_static_offset : TLS_TCB_SIZE;
		o += ((uintptr_t)p->tls.image - o) & (p->tls.align - 1);
		p->tls.offset = o;
		tls_static_offset = o + p->tls.size;
#else
		/* static: place below the previous module (see tls_init.c) */
		size_t o = tls_static_offset + p->tls.size;
		o += (-(uintptr_t)p->tls.image - o) & (p->tls.align - 1);
		p->tls.offset = o;
		tls_static_offset = o;
#endif
		if (p->tls.align > __libc.tls_align) __libc.tls_align = p->tls.align;
		p->tls.next = 0;
		struct tls_module **pp = &__libc.tls_head;
		while (*pp) pp = &(*pp)->next;
		*pp = &p->tls;
		__libc.tls_cnt = id;
	} else {
		p->dyn_tls = 1;
	}
	return 0;
}

/* Slow path of __tls_get_addr: allocate this thread's block for module
 * `id` (and grow its dtv if needed). */
static void *tls_alloc_block(size_t id, size_t off)
{
	struct pthread *self = __self();
	uintptr_t *dtv = self->dtv;
	pthread_mutex_lock(&dl_lock);
	struct tls_module *m = id < tls_mods_cap ? tls_mods[id] : 0;
	size_t cnt = tls_next_id;
	pthread_mutex_unlock(&dl_lock);
	if (!m) a_crash();
	if (dtv[0] < id) {
		/* grow: the old dtv may live inside the static TLS area, so
		 * it is copied, never freed (unless it was allocated here) */
		uintptr_t *nd = calloc(cnt + 2, sizeof *nd);
		if (!nd) a_crash();
		memcpy(nd, dtv, (dtv[0] + 1) * sizeof *nd);
		nd[cnt + 1] = 1;                 /* marker: heap-allocated dtv */
		if (dtv[0] > __libc.tls_cnt && dtv[dtv[0] + 1] == 1) free(dtv);
		nd[0] = cnt;
		self->dtv = dtv = nd;
	}
	if (!dtv[id]) {
		size_t align = m->align < sizeof(void *) ? sizeof(void *) : m->align;
		void *mem;
		if (posix_memalign(&mem, align, m->size)) a_crash();
		memcpy(mem, m->image, m->len);
		memset((char *)mem + m->len, 0, m->size - m->len);
		dtv[id] = (uintptr_t)mem;
	}
	return (char *)dtv[id] + off;
}

void *__tls_get_addr(size_t *ti)
{
	uintptr_t *dtv = __self()->dtv;
	size_t id = ti[0];
	if (likely(id <= dtv[0] && dtv[id])) return (char *)dtv[id] + ti[1];
	return tls_alloc_block(id, ti[1]);
}

/* Free a thread's dynamically allocated TLS blocks (pthread_exit). */
void __tls_thread_exit(struct pthread *self)
{
	uintptr_t *dtv = self->dtv;
	size_t n = dtv[0];
	for (size_t i = __libc.tls_cnt + 1; i <= n; i++) {
		free((void *)dtv[i]);
		dtv[i] = 0;
	}
	if (n > __libc.tls_cnt && dtv[n + 1] == 1) {
		self->dtv = 0;
		free(dtv);
	}
}

/* ----------------------------------------------------- constructors */

static void run_ctors(struct dso *p)
{
	if (p->constructed) return;
	p->constructed = 1;
	for (size_t i = 0; i < p->ndeps; i++) run_ctors(p->deps[i]);
	if (p == head || p == &ldso) return;     /* the program's run from crt1 */
	char **envp = __environ;
	if (p->dyn[DT_INIT]) ((void (*)(void))(p->base + p->dyn[DT_INIT]))();
	if (p->dyn[DT_INIT_ARRAY]) {
		void (**f)(int, char **, char **) = (void *)(p->base + p->dyn[DT_INIT_ARRAY]);
		for (size_t i = 0; i < p->init_array_sz / sizeof *f; i++) f[i](app_argc, app_argv, envp);
	}
	if (p->dyn[DT_FINI] || p->dyn[DT_FINI_ARRAY]) {
		p->fini_next = fini_head;
		fini_head = p;
	}
}

void __libc_exit_fini(void)
{
	for (struct dso *p = fini_head; p; p = p->fini_next) {
		if (p->dyn[DT_FINI_ARRAY]) {
			void (**f)(void) = (void *)(p->base + p->dyn[DT_FINI_ARRAY]);
			for (size_t n = p->fini_array_sz / sizeof *f; n; n--) f[n - 1]();
		}
		if (p->dyn[DT_FINI]) ((void (*)(void))(p->base + p->dyn[DT_FINI]))();
	}
	fini_head = 0;
}

void __ldso_atfork(int who)
{
	if (who < 0) pthread_mutex_lock(&dl_lock);
	else if (who > 0) dl_lock = (pthread_mutex_t)PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
	else pthread_mutex_unlock(&dl_lock);
}

/* ---------------------------------------------------------- startup */

static size_t *saved_sp;

_Noreturn void __dls2(unsigned char *base, size_t *sp)
{
	ldso.base = base;
	const Elf64_Ehdr *eh = (const void *)base;
	ldso.phdr = (const void *)(base + eh->e_phoff);
	ldso.phnum = eh->e_phnum;
	ldso.phentsize = eh->e_phentsize;
	ldso.name = (char *)"libc.so";
	ldso.shortname = "libc.so";
	ldso.refcnt = 1;
	__dls3(sp);
}

static void print_str(int fd, const char *s)
{
	write(fd, s, strlen(s));
}

static char *default_sys_path(void)
{
	/* the directory holding libc.so first, then the conventional ones */
	const char *self = ldso.name;
	const char *slash = strrchr(self, '/');
	const char *tailp = "/usr/local/lib/spfxd:/usr/lib/spfxd:/lib/spfxd";
	size_t dl = slash ? (size_t)(slash - self) : 0;
	char *s = malloc(dl + 1 + strlen(tailp) + 1);
	if (!s) return (char *)tailp;
	memcpy(s, self, dl);
	s[dl] = ':';
	strcpy(s + (dl ? dl + 1 : 0), tailp);
	return s;
}

/* Configuration: <libdir>/../etc/ld-spfxd-x86_64.path, one directory per
 * line or colon separated, replaces the built-in system path. */
static void load_sys_path(void)
{
	const char *self = ldso.name;
	const char *slash = strrchr(self, '/');
	char buf[PATH_MAX];
	if (slash && (size_t)(slash - self) + 32 < sizeof buf) {
		memcpy(buf, self, (size_t)(slash - self));
		strcpy(buf + (slash - self), "/../etc/ld-spfxd-x86_64.path");
		int fd = open(buf, O_RDONLY | O_CLOEXEC);
		if (fd >= 0) {
			char tmp[4096];
			ssize_t l = read(fd, tmp, sizeof tmp - 1);
			close(fd);
			if (l > 0) {
				tmp[l] = 0;
				for (char *c = tmp; *c; c++) if (*c == '\n') *c = ':';
				sys_path = strdup(tmp);
				if (sys_path) return;
			}
		}
	}
	sys_path = default_sys_path();
}

static _Noreturn void jump_to_entry(size_t entry, size_t *sp)
{
	__arch_jump_to_entry(entry, sp);
	for (;;);
}

_Noreturn void __dls3(size_t *sp)
{
	int argc = (int)sp[0];
	char **argv = (char **)(sp + 1);
	char **envp = argv + argc + 1;
	size_t *auxv;
	size_t i;
	for (i = 0; envp[i]; i++);
	auxv = (size_t *)(envp + i + 1);

	/* C runtime with a provisional thread pointer (TLS of the program
	 * only); the final TLS area is installed once all modules are known */
	__init_libc(envp, argv[0]);
	saved_sp = sp;

	for (const Elf64_Phdr *ph = ldso.phdr; ph < ldso.phdr + ldso.phnum; ph++)
		if (ph->p_type == PT_DYNAMIC) ldso.dynv = (void *)(ldso.base + ph->p_vaddr);
	decode_dyn(&ldso);
	scan_phdrs(&ldso);
	ldso.global = 1;
	ldso.relocated = 0;

	size_t aux_phdr = 0, aux_phnum = 0, aux_phent = 0, aux_entry = 0;
	size_t *aux_phdr_p = 0, *aux_phnum_p = 0, *aux_entry_p = 0;
	for (size_t *a = auxv; a[0]; a += 2) {
		if (a[0] == AT_PHDR) { aux_phdr = a[1]; aux_phdr_p = a + 1; }
		else if (a[0] == AT_PHNUM) { aux_phnum = a[1]; aux_phnum_p = a + 1; }
		else if (a[0] == AT_PHENT) aux_phent = a[1];
		else if (a[0] == AT_ENTRY) { aux_entry = a[1]; aux_entry_p = a + 1; }
	}

	if (!__libc.secure) {
		char *e = getenv("LD_LIBRARY_PATH");
		if (e) env_path = e;
		/* the ldd convention: list the dependencies instead of running */
		e = getenv("LD_TRACE_LOADED_OBJECTS");
		if (e && *e && strcmp(e, "0")) ldd_mode = 1;
	}

	struct dso *app;
	const char *argv0 = argv[0] ? argv[0] : "";
	const char *b0 = strrchr(argv0, '/');
	b0 = b0 ? b0 + 1 : argv0;
	if (!strcmp(b0, "ldd")) ldd_mode = 1;

	if (aux_phdr != (size_t)ldso.phdr) {
		/* normal case: the kernel mapped the program */
		app = calloc(1, sizeof *app);
		if (!app) startup_fail("out of memory");
		app->phdr = (const void *)aux_phdr;
		app->phnum = aux_phnum;
		app->phentsize = aux_phent;
		app->kernel_mapped = 1;
		const Elf64_Phdr *ph = app->phdr;
		size_t interp = 0;
		int have_interp = 0;
		for (i = 0; i < app->phnum; i++, ph = (const void *)((const char *)ph + app->phentsize)) {
			if (ph->p_type == PT_PHDR) app->base = (unsigned char *)(aux_phdr - ph->p_vaddr);
			else if (ph->p_type == PT_INTERP) { interp = ph->p_vaddr; have_interp = 1; }
		}
		if (have_interp) ldso.name = (char *)(app->base + interp);
		ldso.shortname = strrchr(ldso.name, '/') ? strrchr(ldso.name, '/') + 1 : ldso.name;
		app->name = (char *)argv0;
		app->shortname = b0;
	} else {
		/* invoked directly: libc.so [--list] [--library-path P] prog args */
		int k = 1;
		ldso.name = (char *)argv0;
		for (; k < argc && argv[k][0] == '-' && argv[k][1] == '-'; k++) {
			if (!strcmp(argv[k], "--list")) ldd_mode = 1;
			else if (!strcmp(argv[k], "--library-path") && k + 1 < argc) env_path = argv[++k];
			else if (!strcmp(argv[k], "--")) { k++; break; }
			else if (!strcmp(argv[k], "--version")) {
				print_str(1, "lib-spfxd C library and dynamic linker (x86-64)\n");
				_exit(0);
			} else {
				break;
			}
		}
		if (k >= argc) {
			print_str(2, "lib-spfxd: usage: libc.so [--list] [--library-path PATH] PROGRAM [ARGS...]\n");
			_exit(1);
		}
		int fd = open(argv[k], O_RDONLY | O_CLOEXEC);
		if (fd < 0) {
			print_str(2, "lib-spfxd: cannot open ");
			print_str(2, argv[k]);
			print_str(2, "\n");
			_exit(127);
		}
		app = alloc_dso(argv[k]);
		if (!app || map_library(fd, app, 1) < 0) {
			print_str(2, "lib-spfxd: not a valid x86-64 ELF program: ");
			print_str(2, argv[k]);
			print_str(2, "\n");
			_exit(127);
		}
		/* entry point from the file header */
		Elf64_Ehdr hdr;
		if (pread(fd, &hdr, sizeof hdr, 0) != sizeof hdr) startup_fail("cannot read program header");
		close(fd);
		aux_entry = (size_t)app->base + hdr.e_entry;
		/* make the auxiliary vector describe the program */
		if (aux_phdr_p) *aux_phdr_p = (size_t)app->phdr;
		if (aux_phnum_p) *aux_phnum_p = app->phnum;
		if (aux_entry_p) *aux_entry_p = aux_entry;
		/* drop our own arguments: shift argc down by k */
		sp += k;
		sp[0] = (size_t)(argc - k);
		argc -= k;
		argv = (char **)(sp + 1);
		program_invocation_name = argv[0];
		const char *s = strrchr(argv[0], '/');
		program_invocation_short_name = s ? (char *)s + 1 : argv[0];
		__libc.progname = program_invocation_short_name;
	}
	app_argc = argc;
	app_argv = argv;
	app->global = 1;
	scan_phdrs(app);
	if (!app->dynv) startup_fail("program has no dynamic section");
	decode_dyn(app);
	load_sys_path();

	/* debugger rendezvous */
	for (Elf64_Dyn *v = app->dynv; v->d_tag; v++)
		if (v->d_tag == DT_DEBUG) v->d_un.d_ptr = (size_t)&_r_debug;
	_r_debug.r_version = 1;
	_r_debug.r_ldbase = (size_t)ldso.base;
	_r_debug.r_brk = (size_t)_dl_debug_state;

	head = tail = 0;
	append(app);

	/* LD_PRELOAD (whitespace or colon separated); in secure mode only
	 * plain names, which are searched in the system path */
	char *pre = getenv("LD_PRELOAD");
	if (pre && *pre) {
		char *copy = strdup(pre);
		for (char *s = copy, *tok; copy && (tok = strtok_r(s, " :\t\n", &s));) {
			if (__libc.secure && strchr(tok, '/')) continue;
			struct dso *p = load_library(tok, app);
			if (!p) {
				print_str(2, "lib-spfxd: warning: cannot preload ");
				print_str(2, tok);
				print_str(2, "\n");
				continue;
			}
			p->global = 1;
		}
	}
	if (load_deps(app) < 0) startup_fail("cannot load dependencies");

	/* libc.so is always in the list and the global scope */
	int have_ldso = 0;
	for (struct dso *p = head; p; p = p->next) if (p == &ldso) have_ldso = 1;
	if (!have_ldso) append(&ldso);
	if (!ldso.deps) ldso.deps = calloc(1, sizeof *ldso.deps);
	for (struct dso *p = head; p; p = p->next) p->global = 1;

	if (ldd_mode) {
		for (struct dso *p = head->next; p; p = p->next) {
			char line[PATH_MAX + 64];
			snprintf(line, sizeof line, "\t%s => %s (%p)\n",
			         p->soname ? p->soname : p->shortname, p->name, (void *)p->base);
			print_str(1, line);
		}
		_exit(0);
	}

	/* static TLS for every module present at startup, in load order */
	__libc.tls_head = 0;
	__libc.tls_cnt = 0;
	__libc.tls_align = 16;
	tls_static_offset = 0;
	tls_next_id = 0;
	for (struct dso *p = head; p; p = p->next)
		if (tls_register(p, 0) < 0) startup_fail("out of memory");

	/* relocate: dependencies before dependents, the program last; then
	 * follow copy relocations and write-protect RELRO */
	for (struct dso *p = tail; p; p = p->prev)
		reloc_dso(p);
	for (struct dso *p = head->next; p; p = p->next) redirect_moved(p);
	for (struct dso *p = head; p; p = p->next) protect_relro(p);

	__tls_layout_finish(tls_static_offset);
	__init_tls_dynamic();

	/* link_map list for debuggers */
	_r_debug.r_map = (struct link_map *)head;
	_r_debug.r_state = RT_CONSISTENT;
	_dl_debug_state();

	__libc.dynamic = 1;
	__libc.initialized = 1;
	runtime = 1;

	/* constructors of the libraries (the program's own run from crt1) */
	run_ctors(app);

	errno = 0;
	jump_to_entry(aux_entry, sp);
}

/* ---------------------------------------------------------- dl API */

static struct dso *addr2dso(size_t a)
{
	for (struct dso *p = head; p; p = p->next) {
		const Elf64_Phdr *ph = p->phdr;
		for (size_t i = 0; i < p->phnum; i++, ph = (const void *)((const char *)ph + p->phentsize)) {
			if (ph->p_type != PT_LOAD) continue;
			size_t lo = (size_t)p->base + ph->p_vaddr;
			if (a >= lo && a - lo < ph->p_memsz) return p;
		}
	}
	return 0;
}

static int valid_handle(void *h)
{
	for (struct dso *p = head; p; p = p->next)
		if (p == h) return 1;
	return 0;
}

static int is_new(struct dso *p, struct dso *orig_tail)
{
	for (struct dso *q = orig_tail->next; q; q = q->next)
		if (q == p) return 1;
	return 0;
}

static void promote_global(struct dso *p)
{
	for (size_t i = 0; i < p->nclosure; i++) p->closure[i]->global = 1;
}

void *dlopen(const char *file, int mode)
{
	if (!file) return head;
	pthread_mutex_lock(&dl_lock);
	struct dso *orig_tail = tail, *p;
	size_t orig_tls_id = tls_next_id;

	if (mode & RTLD_NOLOAD) {
		p = is_libc_name(file) ? &ldso : find_loaded(file);
		if (!p) {
			__dl_seterr("%s: not loaded", file);
		} else {
			if (!p->closure) make_closure(p);
			if (mode & RTLD_GLOBAL) promote_global(p);
			p->refcnt++;
		}
		pthread_mutex_unlock(&dl_lock);
		return p;
	}

	p = load_library(file, head);
	if (!p) {
		__dl_seterr("%s: %s", file, strerror(errno));
		pthread_mutex_unlock(&dl_lock);
		return 0;
	}
	if (!is_new(p, orig_tail)) {
		/* already loaded (at startup or by an earlier dlopen) */
		if (!p->closure && make_closure(p) < 0) {
			__dl_seterr("out of memory");
			pthread_mutex_unlock(&dl_lock);
			return 0;
		}
		p->refcnt++;
		if (mode & RTLD_GLOBAL) promote_global(p);
		pthread_mutex_unlock(&dl_lock);
		return p;
	}
	if (load_deps(p) < 0 || make_closure(p) < 0) goto fail;
	for (struct dso *q = orig_tail->next; q; q = q->next) {
		if (q != p) {
			/* new dependencies resolve within the opened object's scope */
			q->closure = p->closure;
			q->nclosure = p->nclosure;
		}
		if (tls_register(q, 1) < 0) {
			__dl_seterr("out of memory");
			goto fail;
		}
	}
	for (struct dso *q = tail; q != orig_tail; q = q->prev)
		if (reloc_dso(q) < 0) goto fail;
	for (struct dso *q = orig_tail->next; q; q = q->next)
		if (protect_relro(q) < 0) goto fail;
	if (mode & RTLD_GLOBAL) promote_global(p);

	_r_debug.r_state = RT_ADD;
	_dl_debug_state();
	_r_debug.r_state = RT_CONSISTENT;
	_dl_debug_state();
	dl_adds++;
	pthread_mutex_unlock(&dl_lock);

	run_ctors(p);
	return p;

fail:
	/* unmap everything loaded by this call */
	{
		struct dso **shared = p ? p->closure : 0;
		while (tail != orig_tail) {
			struct dso *q = tail;
			tail = q->prev;
			tail->next = 0;
			if (q->tls_id && q->tls_id < tls_mods_cap) tls_mods[q->tls_id] = 0;
			if (q->map) munmap(q->map, q->map_len);
			free(q->deps);
			if (q == p) free(shared);
			free(q->origin);
			free(q);
		}
	}
	tls_next_id = orig_tls_id;
	pthread_mutex_unlock(&dl_lock);
	return 0;
}

int dlclose(void *h)
{
	pthread_mutex_lock(&dl_lock);
	int ok = valid_handle(h) || h == &ldso;
	if (ok && ((struct dso *)h)->refcnt > 1) ((struct dso *)h)->refcnt--;
	pthread_mutex_unlock(&dl_lock);
	if (!ok) {
		__dl_seterr("invalid handle %p passed to dlclose", h);
		return -1;
	}
	return 0;
}

static void *sym_addr(struct symdef def)
{
	size_t v = (size_t)def.dso->base + def.sym->st_value;
	if (ELF64_ST_TYPE(def.sym->st_info) == STT_TLS) {
		size_t ti[2] = { def.dso->tls_id, def.sym->st_value };
		return __tls_get_addr(ti);
	}
	if (ELF64_ST_TYPE(def.sym->st_info) == STT_GNU_IFUNC) return (void *)((size_t (*)(void))v)();
	return (void *)v;
}

static void *do_dlsym(void *h, const char *s, void *ra)
{
	struct symdef def;
	pthread_mutex_lock(&dl_lock);
	if (h == RTLD_DEFAULT || h == head) {
		def = find_sym(s, 0, 0, 0);
	} else if (h == RTLD_NEXT) {
		struct dso *caller = addr2dso((size_t)ra);
		if (!caller) caller = head;
		def.sym = 0;
		uint32_t gh = gnu_hash(s), sh = 0;
		int have_sh = 0;
		for (struct dso *d = caller->next; d && !def.sym; d = d->next) {
			const Elf64_Sym *sym = lookup_in(d, s, gh, &sh, &have_sh);
			if (sym) def = (struct symdef){ sym, d };
		}
	} else if (valid_handle(h) || h == &ldso) {
		struct dso *p = h;
		def = p->closure ? find_sym_list(s, p->closure, p->nclosure) : find_sym_list(s, &p, 1);
	} else {
		pthread_mutex_unlock(&dl_lock);
		__dl_seterr("invalid handle %p passed to dlsym", h);
		return 0;
	}
	pthread_mutex_unlock(&dl_lock);
	if (!def.sym) {
		__dl_seterr("symbol not found: %s", s);
		return 0;
	}
	return sym_addr(def);
}

void *dlsym(void *restrict h, const char *restrict s)
{
	return do_dlsym(h, s, __builtin_return_address(0));
}

/* Number of entries in the dynamic symbol table. */
static size_t count_syms(const struct dso *p)
{
	if (p->hashtab) return p->hashtab[1];
	if (!p->ghashtab) return 0;
	const uint32_t *gh = p->ghashtab;
	uint32_t nbuckets = gh[0], symoff = gh[1], bloom_size = gh[2];
	const uint32_t *buckets = (const void *)((const uint64_t *)(gh + 4) + bloom_size);
	const uint32_t *chain = buckets + nbuckets;
	uint32_t maxi = 0;
	for (uint32_t i = 0; i < nbuckets; i++) if (buckets[i] > maxi) maxi = buckets[i];
	if (maxi < symoff) return symoff;
	while (!(chain[maxi - symoff] & 1)) maxi++;
	return maxi + 1;
}

int dladdr(const void *addr, Dl_info *info)
{
	size_t a = (size_t)addr;
	pthread_mutex_lock(&dl_lock);
	struct dso *p = addr2dso(a);
	if (!p) {
		pthread_mutex_unlock(&dl_lock);
		return 0;
	}
	info->dli_fname = p->name;
	info->dli_fbase = p->map ? p->map : p->base;
	info->dli_sname = 0;
	info->dli_saddr = 0;
	size_t n = count_syms(p), best = 0;
	const Elf64_Sym *bs = 0;
	for (size_t i = 1; i < n; i++) {
		const Elf64_Sym *s = p->syms + i;
		int t = ELF64_ST_TYPE(s->st_info);
		if (s->st_shndx == SHN_UNDEF || !s->st_value || (t != STT_FUNC && t != STT_OBJECT && t != STT_GNU_IFUNC))
			continue;
		size_t v = (size_t)p->base + s->st_value;
		if (v > a) continue;
		if (s->st_size && a >= v + s->st_size) continue;
		if (v >= best) {
			best = v;
			bs = s;
		}
	}
	if (bs) {
		info->dli_sname = p->strings + bs->st_name;
		info->dli_saddr = (void *)best;
	}
	pthread_mutex_unlock(&dl_lock);
	return 1;
}

int dl_iterate_phdr(int (*cb)(struct dl_phdr_info *, size_t, void *), void *data)
{
	int ret = 0;
	pthread_mutex_lock(&dl_lock);
	for (struct dso *p = head; p; p = p->next) {
		struct dl_phdr_info info;
		info.dlpi_addr = (size_t)p->base;
		info.dlpi_name = p == head ? "" : p->name;
		info.dlpi_phdr = p->phdr;
		info.dlpi_phnum = (Elf64_Half)p->phnum;
		info.dlpi_adds = dl_adds + 1;
		info.dlpi_subs = 0;
		info.dlpi_tls_modid = p->tls_id;
		info.dlpi_tls_data = 0;
		if (p->tls_id) {
			uintptr_t *dtv = __self()->dtv;
			if (p->tls_id <= dtv[0]) info.dlpi_tls_data = (void *)dtv[p->tls_id];
		}
		ret = cb(&info, sizeof info, data);
		if (ret) break;
	}
	pthread_mutex_unlock(&dl_lock);
	return ret;
}

int _dl_find_object(void *pc, struct dl_find_object *res)
{
	size_t a = (size_t)pc;
	int ret = -1;
	pthread_mutex_lock(&dl_lock);
	struct dso *p = addr2dso(a);
	if (p) {
		size_t lo = SIZE_MAX, hi = 0;
		void *eh = 0;
		const Elf64_Phdr *ph = p->phdr;
		for (size_t i = 0; i < p->phnum; i++, ph = (const void *)((const char *)ph + p->phentsize)) {
			if (ph->p_type == PT_LOAD) {
				if (ph->p_vaddr < lo) lo = ph->p_vaddr;
				if (ph->p_vaddr + ph->p_memsz > hi) hi = ph->p_vaddr + ph->p_memsz;
			} else if (ph->p_type == PT_GNU_EH_FRAME) {
				eh = p->base + ph->p_vaddr;
			}
		}
		memset(res, 0, sizeof *res);
		res->dlfo_map_start = p->base + lo;
		res->dlfo_map_end = p->base + hi;
		res->dlfo_link_map = (struct link_map *)p;
		res->dlfo_eh_frame = eh;
		ret = 0;
	}
	pthread_mutex_unlock(&dl_lock);
	return ret;
}
