# lib-spfxd benchmarks: the same source built against lib-spfxd and the
# host C library, run back to back, results joined by compare.sh.
#
#   make -C bench run [FILTER=name]       (or `make bench` from the top)
#   make -C bench run STATIC=1            static links for both

TOP     := $(abspath ..)
SPFXDCC := $(TOP)/lib/spfxd-gcc
HOSTCC  ?= cc
CFLAGS  := -O2 -g -Wall
OUT     := out
LINK    := $(if $(STATIC),-static,)

.PHONY: run clean
run: $(OUT)/bench-spfxd $(OUT)/bench-host
	@$(OUT)/bench-spfxd $(FILTER) > $(OUT)/spfxd.txt
	@$(OUT)/bench-host $(FILTER) > $(OUT)/host.txt
	@sh compare.sh $(OUT)/spfxd.txt $(OUT)/host.txt | tee $(OUT)/results.txt

$(OUT)/bench-spfxd: bench.c $(TOP)/lib/libc.a
	@mkdir -p $(OUT)
	$(SPFXDCC) $(CFLAGS) $(LINK) -o $@ bench.c -lm -lpthread

$(OUT)/bench-host: bench.c
	@mkdir -p $(OUT)
	$(HOSTCC) $(CFLAGS) $(LINK) -o $@ bench.c -lm -lpthread

clean:
	rm -rf $(OUT)
