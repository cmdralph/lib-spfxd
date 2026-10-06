# lib-spfxd — build system
#
#   make                 build lib/libc.a, lib/libc.so, startup objects, wrapper
#   make check           build and run the test suite (static + dynamic)
#   make bench           build and run benchmarks against lib-spfxd and the host libc
#   make audit           symbol/ELF/dependency verification
#   make install PREFIX=/opt/spfxd
#
# Layout:
#   include/                 portable public headers
#   arch/$(ARCH)/include/    architecture-specific public headers (bits/)
#   arch/$(ARCH)/internal/   architecture-specific internal headers
#   arch/$(ARCH)/src/<dir>/  architecture-specific sources; a file here
#                            replaces src/<dir>/<same name>.c
#   src/<subsystem>/         portable implementation
#   crt/                     program entry objects
#   ldso/                    dynamic linker (built into libc.so)

ARCH     ?= x86_64
CC       ?= gcc
AR       ?= ar
RANLIB   ?= ranlib
PREFIX   ?= /usr/local/spfxd
O        := build

ifeq ($(CC),cc)
CC := gcc
endif

# ---------------------------------------------------------------------------
# Flags
# ---------------------------------------------------------------------------
OPT      ?= -O2
WARN     := -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
            -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wvla \
            -Wno-sign-compare -Werror=implicit-function-declaration \
            -Werror=return-type -Werror=incompatible-pointer-types
INCS     := -Iarch/$(ARCH)/internal -Isrc/internal -Iinclude -Iarch/$(ARCH)/include

# -ffreestanding:   no assumptions about a hosted environment (and no host headers)
# -fno-stack-protector: the canary lives in the TCB, which does not exist yet
#                   during startup; libc itself is built without it
# -fno-tree-loop-distribute-patterns: never turn loops back into memcpy/memset
#                   calls (memcpy must not call itself)
# -ffp-contract=off: double-double arithmetic in libm relies on exact rounding
#                   of every operation, so no implicit FMA contraction
# -fno-plt / -fno-semantic-interposition are left to the defaults so that
#                   public symbols remain interposable in libc.so
BASEFLAGS := -std=gnu11 -nostdinc -ffreestanding -fno-stack-protector \
            -fno-tree-loop-distribute-patterns -ffp-contract=off \
            -fexcess-precision=standard -frounding-math -fno-strict-overflow \
            -ffunction-sections -fdata-sections -fno-common \
            -fasynchronous-unwind-tables -pipe -D_XOPEN_SOURCE=700 -D_GNU_SOURCE \
            -U_FORTIFY_SOURCE -fcf-protection=none
ifeq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
BASEFLAGS += -fno-stack-clash-protection
endif
CFLAGS_LIB  = $(BASEFLAGS) $(OPT) $(WARN) $(INCS) -MMD -MP $(EXTRA_CFLAGS)
ASFLAGS_LIB = $(INCS) -nostdinc -MMD -MP

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
ARCH_SRCS := $(sort $(wildcard arch/$(ARCH)/src/*/*.c arch/$(ARCH)/src/*/*.S))
ARCH_KEYS := $(basename $(patsubst arch/$(ARCH)/src/%,%,$(ARCH_SRCS)))
PORT_SRCS := $(filter-out $(addprefix src/,$(addsuffix .c,$(ARCH_KEYS))), \
               $(sort $(wildcard src/*/*.c)))
LIB_SRCS  := $(PORT_SRCS) $(ARCH_SRCS)
LDSO_SRCS := $(sort $(wildcard ldso/*.c))

objs_of    = $(patsubst %,$(O)/$(1)/%.o,$(basename $(2)))
STATIC_OBJS := $(call objs_of,static,$(LIB_SRCS))
SHARED_OBJS := $(call objs_of,shared,$(LIB_SRCS) $(LDSO_SRCS))

CRT_OBJS := lib/crt1.o lib/Scrt1.o lib/rcrt1.o lib/crti.o lib/crtn.o
EMPTY_LIBS := lib/libm.a lib/libpthread.a lib/librt.a lib/libdl.a lib/libutil.a \
              lib/libxnet.a lib/libcrypt.a lib/libresolv.a

LIBGCC := $(shell $(CC) -print-libgcc-file-name)

ALL := lib/libc.a lib/libc.so $(CRT_OBJS) $(EMPTY_LIBS) lib/spfxd-gcc lib/spfxd-gcc.specs

.PHONY: all clean check check-static check-dynamic bench audit install headers-check
all: $(ALL)

# ---------------------------------------------------------------------------
# Object rules
# ---------------------------------------------------------------------------
$(O)/static/%.o: %.c
	@mkdir -p $(@D)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS_LIB) -fPIE -c -o $@ $<

$(O)/static/%.o: %.S
	@mkdir -p $(@D)
	@echo "  AS      $<"
	@$(CC) $(ASFLAGS_LIB) -fPIE -c -o $@ $<

$(O)/shared/%.o: %.c
	@mkdir -p $(@D)
	@echo "  CC [pic] $<"
	@$(CC) $(CFLAGS_LIB) -fPIC -DSPFXD_SHARED -c -o $@ $<

$(O)/shared/%.o: %.S
	@mkdir -p $(@D)
	@echo "  AS [pic] $<"
	@$(CC) $(ASFLAGS_LIB) -fPIC -DSPFXD_SHARED -c -o $@ $<

# Startup code that runs before relocation or TLS setup must not use
# anything that needs either; ldso and crt are compiled position-independent.
$(O)/shared/ldso/%.o: ldso/%.c
	@mkdir -p $(@D)
	@echo "  CC [ldso] $<"
	@$(CC) $(CFLAGS_LIB) -fPIC -DSPFXD_SHARED -fno-asynchronous-unwind-tables -c -o $@ $<

# Header dependencies: generated per object by -MMD (public and internal
# headers); every object also depends on the Makefile itself.
INTERNAL_HDRS := $(wildcard src/internal/*.h arch/$(ARCH)/internal/*.h)
$(STATIC_OBJS) $(SHARED_OBJS): Makefile
-include $(STATIC_OBJS:.o=.d) $(SHARED_OBJS:.o=.d)

# ---------------------------------------------------------------------------
# Libraries
# ---------------------------------------------------------------------------
lib/libc.a: $(STATIC_OBJS)
	@mkdir -p lib
	@echo "  AR      $@"
	@rm -f $@
	@$(AR) rc $@ $(STATIC_OBJS)
	@$(RANLIB) $@

# libc.so is also the dynamic linker (its ELF entry point is the loader).
lib/libc.so: $(SHARED_OBJS) arch/$(ARCH)/libc.map
	@mkdir -p lib
	@echo "  LD      $@"
	@$(CC) -nostdlib -shared -Wl,-e,_dlstart -Wl,-Bsymbolic-functions \
		-Wl,--hash-style=both -Wl,-soname,libc.so -Wl,-z,now -Wl,-z,relro \
		-Wl,--gc-sections -Wl,--version-script,arch/$(ARCH)/libc.map \
		-o $@ $(SHARED_OBJS) $(LIBGCC)

$(EMPTY_LIBS):
	@mkdir -p lib
	@rm -f $@
	@$(AR) rc $@

lib/crt1.o: crt/crt1.c $(INTERNAL_HDRS)
	@mkdir -p lib
	@echo "  CC      $@"
	@$(CC) $(CFLAGS_LIB) -fno-asynchronous-unwind-tables -fPIE -c -o $@ $<

lib/Scrt1.o: crt/crt1.c $(INTERNAL_HDRS)
	@mkdir -p lib
	@echo "  CC      $@"
	@$(CC) $(CFLAGS_LIB) -fno-asynchronous-unwind-tables -fPIC -c -o $@ $<

lib/rcrt1.o: crt/rcrt1.c crt/crt1.c ldso/reloc_self.h $(INTERNAL_HDRS)
	@mkdir -p lib
	@echo "  CC      $@"
	@$(CC) $(CFLAGS_LIB) -fno-asynchronous-unwind-tables -fPIC -Ildso -c -o $@ $<

lib/crti.o: arch/$(ARCH)/crt/crti.S
	@mkdir -p lib
	@$(CC) $(ASFLAGS_LIB) -c -o $@ $<

lib/crtn.o: arch/$(ARCH)/crt/crtn.S
	@mkdir -p lib
	@$(CC) $(ASFLAGS_LIB) -c -o $@ $<

lib/spfxd-gcc.specs: tools/spfxd-gcc.specs.in
	@mkdir -p lib
	@sed -e "s|@INC@|$(abspath include)|g" -e "s|@ARCHINC@|$(abspath arch/$(ARCH)/include)|g" \
	     -e "s|@LIB@|$(abspath lib)|g" -e "s|@CCINC@|$(shell $(CC) -print-file-name=include)|g" $< > $@

lib/spfxd-gcc: tools/spfxd-gcc.in lib/spfxd-gcc.specs
	@mkdir -p lib
	@sed -e "s|@LIB@|$(abspath lib)|g" -e "s|@CC@|$(CC)|g" $< > $@
	@chmod +x $@

# ---------------------------------------------------------------------------
# Tests, benchmarks, audit
# ---------------------------------------------------------------------------
check: all
	@$(MAKE) --no-print-directory -C tests run

check-static: all
	@$(MAKE) --no-print-directory -C tests run MODES=static

check-dynamic: all
	@$(MAKE) --no-print-directory -C tests run MODES=dynamic

bench: all
	@$(MAKE) --no-print-directory -C bench run

audit: all
	@sh tools/audit.sh

headers-check: all
	@sh tools/headers-check.sh

# ---------------------------------------------------------------------------
install: all
	install -d $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include $(DESTDIR)$(PREFIX)/bin
	cp -R include/. $(DESTDIR)$(PREFIX)/include/
	cp -R arch/$(ARCH)/include/. $(DESTDIR)$(PREFIX)/include/
	install -m 644 lib/libc.a lib/*.o $(EMPTY_LIBS) $(DESTDIR)$(PREFIX)/lib/
	install -m 755 lib/libc.so $(DESTDIR)$(PREFIX)/lib/
	sed -e "s|@INC@|$(PREFIX)/include|g" -e "s|@ARCHINC@|$(PREFIX)/include|g" \
	    -e "s|@LIB@|$(PREFIX)/lib|g" -e "s|@CCINC@|$(shell $(CC) -print-file-name=include)|g" \
	    tools/spfxd-gcc.specs.in > $(DESTDIR)$(PREFIX)/lib/spfxd-gcc.specs
	sed -e "s|@LIB@|$(PREFIX)/lib|g" -e "s|@CC@|$(CC)|g" tools/spfxd-gcc.in > $(DESTDIR)$(PREFIX)/bin/spfxd-gcc
	chmod 755 $(DESTDIR)$(PREFIX)/bin/spfxd-gcc

clean:
	rm -rf $(O) lib
	@$(MAKE) --no-print-directory -C tests clean
	@$(MAKE) --no-print-directory -C bench clean
