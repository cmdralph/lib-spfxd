#!/bin/sh
# lib-spfxd — build and binary audit.
#
#   tools/audit.sh [--quick]
#
# Verifies, with nm/readelf/objdump/ldd-style tracing, that the build is
# warning-free and that the outputs are self-contained:
#
#   build     a full rebuild produces no compiler or assembler warnings
#             (skipped with --quick)
#   libc.so   no DT_NEEDED, no undefined dynamic symbols, no TEXTREL,
#             BIND_NOW + RELRO, non-executable stack, no glibc symbol
#             versions, no internal helpers exported
#   libc.a    every undefined reference is satisfied inside the archive,
#             except linker-defined symbols and libgcc helpers
#   crt       crt1.o/Scrt1.o/rcrt1.o reference only main, the startup
#             entry and linker-defined symbols; crti.o/crtn.o nothing
#   programs  a static and a dynamic test program link only lib-spfxd:
#             no PT_INTERP / dynamic section when static; interpreter
#             lib/libc.so and DT_NEEDED libc.so only when dynamic; the
#             loader's trace lists nothing else; both run correctly
#   headers   include/ never reaches for host headers (#include_next,
#             /usr/include) and every header compiles on its own
#   sources   no copyright notices of other C libraries in the tree
#
# Exit status 0 when every check passes.
cd "$(dirname "$0")/.." || exit 1
TOP=$(pwd)
QUICK=0
[ "$1" = "--quick" ] && QUICK=1
TMP=$(mktemp -d "${TMPDIR:-/tmp}/spfxd-audit.XXXXXX") || exit 1
trap 'rm -rf "$TMP"' EXIT
pass=0; fail=0

ok()  { pass=$((pass + 1)); printf '  PASS  %s\n' "$1"; }
bad() { fail=$((fail + 1)); printf '  FAIL  %s\n' "$1"; [ -n "$2" ] && printf '%s\n' "$2" | sed 's/^/        /' | head -20; }
check() { # description, command producing offending lines (empty = pass)
	out=$(eval "$2" 2>&1)
	if [ -z "$out" ]; then ok "$1"; else bad "$1" "$out"; fi
}

echo "== build"
if [ $QUICK = 1 ]; then
	echo "  skip  full rebuild (--quick)"
	make -s -j"$(nproc)" >/dev/null 2>&1 || { bad "make"; exit 1; }
else
	make -s clean >/dev/null 2>&1
	make -j"$(nproc)" >"$TMP/build.log" 2>&1 || { bad "make" "$(tail -20 "$TMP/build.log")"; exit 1; }
	check "full rebuild without warnings" "grep -iE 'warning:|error:' '$TMP/build.log'"
fi
for f in lib/libc.a lib/libc.so lib/crt1.o lib/Scrt1.o lib/rcrt1.o lib/crti.o lib/crtn.o; do
	[ -f "$f" ] || bad "output $f exists"
done

echo "== libc.so"
SO=lib/libc.so
check "no DT_NEEDED entries" "readelf -dW $SO | grep NEEDED"
check "no undefined dynamic symbols" "readelf --dyn-syms -W $SO | awk 'NR>3 && \$7==\"UND\" && \$8!=\"\"'"
check "no text relocations" "readelf -dW $SO | grep -E 'TEXTREL'"
check "BIND_NOW" "readelf -dW $SO | grep -qE 'BIND_NOW|FLAGS_1.*NOW' || echo missing"
check "RELRO segment" "readelf -lW $SO | grep -q GNU_RELRO || echo missing"
check "non-executable stack" "readelf -lW $SO | awk '/GNU_STACK/ && \$7 ~ /E/'"
check "no symbol version requirements (no glibc)" "readelf -VW $SO | grep -iE 'GLIBC|verneed|Version needs'"
# (the loader's alias table names libc.so.6, libm.so.6, ld-linux-x86-64.so.2
# etc. so that requests for them resolve to lib-spfxd itself; those
# strings are expected)
check "no glibc symbol-version strings" "strings -a $SO | grep -E 'GLIBC_[0-9]'"
# exported names beginning with "__" must be ABI symbols, not internals
readelf --dyn-syms -W $SO | awk 'NR>3 && $7!="UND" && $5!="LOCAL" {print $8}' | sed 's/@.*//' | sort -u >"$TMP/exports"
check "no internal helpers exported" "grep '^__' '$TMP/exports' | grep -vxE '___environ|__environ|__assert_fail|__cxa_atexit|__cxa_finalize|__errno_location|__h_errno_location|__libc_start_main|__libc_current_sigrtm(ax|in)|__stack_chk_fail|__tls_get_addr|__sigsetjmp|__sched_cpucount|__xpg_basename|__xpg_strerror_r|__gnu_strerror_r|__f[a-z]+|__(finite|isinf|isnan|signbit|fpclassify)[fl]?|__spfxd_(cleanup_push|cleanup_pop|ctype_tab|flt_rounds|mb_cur_max)'"
printf '  info  %s exported symbols\n' "$(wc -l <"$TMP/exports")"

echo "== libc.a"
nm -A lib/libc.a 2>/dev/null | awk '$(NF-1)=="U" {print $NF}' | sort -u >"$TMP/und"
nm lib/libc.a 2>/dev/null | awk 'NF==3 && $2 ~ /^[TDBRWVGSAtdbrwvgsai]$/ {print $3}' | sort -u >"$TMP/def"
check "all references resolved within the archive" "comm -23 '$TMP/und' '$TMP/def' | grep -vxE '_GLOBAL_OFFSET_TABLE_|__popcount[sd]i2|__(u?div|u?mod|mul)[st]i3|__[a-z]+[sdtx]f[23]'"
check "no glibc symbol references in objects" "nm lib/libc.a 2>/dev/null | grep -E '@GLIBC|@@GLIBC'"

echo "== crt objects"
for o in crt1.o Scrt1.o rcrt1.o; do
	check "$o references only startup symbols" "nm -u lib/$o | awk '{print \$2}' | grep -vxE 'main|__libc_start_main|_init|_fini|_DYNAMIC|_GLOBAL_OFFSET_TABLE_|__(pre)?init_array_(start|end)|__fini_array_(start|end)'"
done
for o in crti.o crtn.o; do
	check "$o has no undefined references" "nm -u lib/$o 2>/dev/null"
done
for o in crt1.o Scrt1.o rcrt1.o crti.o crtn.o; do
	check "$o marks a non-executable stack" "readelf -SW lib/$o | grep -q 'GNU-stack' || echo missing"
done

echo "== test programs"
cat >"$TMP/t.c" <<'EOF'
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void *th(void *a) { return (char *)a + 1; }
int main(void)
{
	pthread_t t;
	void *r;
	char *p = malloc(64);
	strcpy(p, "lib-spfxd");
	pthread_create(&t, 0, th, p);
	pthread_join(t, &r);
	printf("%s %d %.6f\n", (char *)r, (int)strlen(p), sqrt(2.0));
	free(p);
	return 0;
}
EOF
WANT="ib-spfxd 9 1.414214"
if lib/spfxd-gcc -static -O2 -o "$TMP/t_static" "$TMP/t.c" -lm -lpthread 2>"$TMP/err"; then
	check "static: no PT_INTERP" "readelf -lW '$TMP/t_static' | grep INTERP"
	check "static: no dynamic section" "readelf -dW '$TMP/t_static' | grep -v 'no dynamic section' | grep -E 'NEEDED|SONAME'"
	check "static: no glibc strings" "strings -a '$TMP/t_static' | grep -E 'GLIBC_[0-9]|ld-linux'"
	check "static: runs" "[ \"\$('$TMP/t_static')\" = '$WANT' ] || echo 'unexpected output'"
else
	bad "static: link" "$(cat "$TMP/err")"
fi
if lib/spfxd-gcc -O2 -o "$TMP/t_dyn" "$TMP/t.c" -lm -lpthread 2>"$TMP/err"; then
	check "dynamic: interpreter is lib/libc.so" "readelf -lW '$TMP/t_dyn' | grep 'interpreter' | grep -v '$TOP/lib/libc.so'"
	check "dynamic: DT_NEEDED is libc.so only" "readelf -dW '$TMP/t_dyn' | awk '/NEEDED/' | grep -v '\\[libc.so\\]'"
	check "dynamic: no glibc symbol versions" "readelf -VW '$TMP/t_dyn' | grep -i GLIBC"
	check "dynamic: loader trace lists only libc.so" "LD_TRACE_LOADED_OBJECTS=1 '$TMP/t_dyn' | grep -v '^[[:space:]]*libc.so => $TOP/lib/libc.so'"
	check "dynamic: runs" "[ \"\$('$TMP/t_dyn')\" = '$WANT' ] || echo 'unexpected output'"
	check "dynamic: objdump finds no glibc PLT targets" "objdump -d '$TMP/t_dyn' | grep -E '@GLIBC'"
else
	bad "dynamic: link" "$(cat "$TMP/err")"
fi

echo "== headers"
check "no #include_next or host include paths" "grep -rnE '#[[:space:]]*include_next|/usr/include' include arch/*/include"
if [ -x tools/headers-check.sh ] || [ -f tools/headers-check.sh ]; then
	if sh tools/headers-check.sh >"$TMP/hc.log" 2>&1; then ok "every header compiles on its own"
	else bad "every header compiles on its own" "$(tail -20 "$TMP/hc.log")"; fi
fi

echo "== sources"
check "no other C library's copyright notices" "grep -rniE 'free software foundation|rich felker|sun microsystems|regents of the university of california|freebsd|netbsd|openbsd|android open source|newlib|uclibc|dietlibc|cosmopolitan|llvm-project' src include arch crt ldso"

echo "== $pass passed, $fail failed"
[ $fail = 0 ]
