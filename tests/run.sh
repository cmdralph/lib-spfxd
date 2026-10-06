#!/bin/sh
# lib-spfxd test runner.
#
#   tests/run.sh [static] [dynamic]        (default: both)
#
# unit/*.c     built with lib/spfxd-gcc and run; must exit 0
# oracle/*.c   built with the host compiler/C library and with lib-spfxd;
#              the two outputs must be byte-identical
# dynamic/     shared libraries and dlopen (dynamic mode only)
# math/        libm accuracy oracle (requires python3 with mpmath)
#
# Logs and binaries go to tests/out/.
cd "$(dirname "$0")" || exit 1
TOP=$(cd .. && pwd)
CC="$TOP/lib/spfxd-gcc"
HOSTCC=${HOSTCC:-cc}
OUT="$TOP/tests/out"
CFLAGS="-O2 -g -Wall -Wno-unused-result -D_GNU_SOURCE -Icommon"
TIMEOUT=${TIMEOUT:-300}
MODES=${*:-static dynamic}
mkdir -p "$OUT"
pass=0; fail=0; failed=""

record() { # name status
	if [ "$2" = 0 ]; then pass=$((pass + 1)); printf '  PASS  %s\n' "$1"
	else fail=$((fail + 1)); failed="$failed $1"; printf '  FAIL  %s\n' "$1"; fi
}

for mode in $MODES; do
	case $mode in static) MFLAG=-static ;; dynamic) MFLAG= ;; *) echo "unknown mode $mode"; exit 2 ;; esac
	echo "== $mode =="
	mkdir -p "$OUT/$mode"
	for src in unit/*.c; do
		n=$(basename "$src" .c)
		bin="$OUT/$mode/$n"
		if ! "$CC" $MFLAG $CFLAGS -o "$bin" "$src" -lm -lpthread >"$bin.build.log" 2>&1; then
			cat "$bin.build.log"; record "$mode/unit/$n (build)" 1; continue
		fi
		(cd "$OUT/$mode" && timeout "$TIMEOUT" "$bin" >"$bin.log" 2>&1); st=$?
		[ $st != 0 ] && tail -20 "$bin.log"
		record "$mode/unit/$n" $st
	done
	for src in oracle/*.c; do
		n=$(basename "$src" .c)
		ref="$OUT/host-$n"
		if [ ! -x "$ref" ] || [ "$src" -nt "$ref" ]; then
			"$HOSTCC" -O1 $CFLAGS -o "$ref" "$src" -lm >/dev/null 2>&1 || { echo "  (host build of $n failed, skipped)"; continue; }
		fi
		bin="$OUT/$mode/oracle-$n"
		if ! "$CC" $MFLAG -O1 $CFLAGS -o "$bin" "$src" -lm >"$bin.build.log" 2>&1; then
			cat "$bin.build.log"; record "$mode/oracle/$n (build)" 1; continue
		fi
		timeout "$TIMEOUT" "$ref" >"$ref.out" 2>&1
		timeout "$TIMEOUT" "$bin" >"$bin.out" 2>&1
		if cmp -s "$ref.out" "$bin.out"; then record "$mode/oracle/$n" 0
		else diff "$ref.out" "$bin.out" | head -20; record "$mode/oracle/$n" 1; fi
	done
	if [ $mode = dynamic ]; then
		d="$OUT/dynamic/dl"; mkdir -p "$d"
		if "$CC" -O2 -fPIC -shared -o "$d/libdep.so" dynamic/libdep.c &&
		   "$CC" -O2 -fPIC -shared -o "$d/libmid.so" dynamic/libmid.c -L"$d" -ldep &&
		   "$CC" -O2 -fPIC -shared -o "$d/libplug.so" dynamic/libplug.c &&
		   "$CC" -O2 -rdynamic -o "$d/dyn_main" dynamic/dyn_main.c -L"$d" -lmid -ldep -Wl,-rpath,'$ORIGIN' -lpthread; then
			timeout "$TIMEOUT" "$d/dyn_main" >"$d/dyn_main.log" 2>&1; st=$?
			[ $st != 0 ] && tail -20 "$d/dyn_main.log"
			record "dynamic/dlopen" $st
		else
			record "dynamic/dlopen (build)" 1
		fi
	fi
done

if python3 -c "import mpmath" 2>/dev/null; then
	echo "== libm accuracy =="
	"$CC" -static -O2 -o "$OUT/ulp_driver" math/ulp_driver.c -lm &&
	python3 math/ulp_check.py "$OUT/ulp_driver" >"$OUT/ulp.log" 2>&1; st=$?
	grep -c ok "$OUT/ulp.log" | sed 's/^/  cases ok: /'
	[ $st != 0 ] && grep FAIL "$OUT/ulp.log"
	record "math/ulp" $st
else
	echo "== libm accuracy: skipped (python3 mpmath not available) =="
fi

echo "== $pass passed, $fail failed =="
[ $fail = 0 ] || { echo "failed:$failed"; exit 1; }
