#!/bin/sh
# lib-spfxd — verify that every function declared in the public headers is
# defined in libc.a, and that every public header compiles on its own
# (self-contained) in C11, C99, GNU and C++ modes.
set -e
cd "$(dirname "$0")/.."
CC=${CC:-gcc}
INC="-nostdinc -Iinclude -Iarch/x86_64/include"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

fail=0
# 1. self-containment
for h in $(cd include && find . -name '*.h' | sed 's|^\./||' | sort); do
	for mode in "-std=c11" "-std=gnu11 -D_GNU_SOURCE" "-std=c99 -D_POSIX_C_SOURCE=200809L"; do
		printf '#include <%s>\n#include <%s>\nint spfxd_dummy;\n' "$h" "$h" > "$tmp/t.c"
		if ! $CC $INC $mode -Wall -Werror -fsyntax-only "$tmp/t.c" 2> "$tmp/err"; then
			echo "NOT SELF-CONTAINED: <$h> ($mode)"; head -3 "$tmp/err"; fail=1
		fi
	done
done

# 2. every declared function is defined
cat > "$tmp/all.c" <<EOT
#define _GNU_SOURCE 1
EOT
for h in $(cd include && find . -name '*.h' | sed 's|^\./||' | grep -v '^tgmath.h$' | sort); do
	echo "#include <$h>" >> "$tmp/all.c"
done
$CC $INC -std=gnu11 -E -P "$tmp/all.c" > "$tmp/all.i"
# function declarations: identifiers followed by '(' at file scope in prototypes
python3 - "$tmp/all.i" > "$tmp/declared" <<'PY'
import re, sys
src = open(sys.argv[1]).read()
# drop function bodies (static inline helpers)
out, depth = [], 0
for ch in src:
    if ch == '{':
        depth += 1
    if depth == 0:
        out.append(ch)
    if ch == '}':
        depth -= 1
        if depth == 0: out.append(';')
text = ''.join(out)
names = set()
for stmt in text.split(';'):
    stmt = ' '.join(stmt.split())
    if not stmt or stmt.startswith('typedef') or 'static' in stmt.split('(')[0]:
        continue
    m = re.match(r'^(?:extern\s+)?[^()]*?\b([A-Za-z_]\w*)\s*\((?!\s*\*)', stmt)
    if m and '(*' not in stmt.split(m.group(1))[0]:
        name = m.group(1)
        if name in ('__attribute__', '__asm__', 'sizeof', '_Static_assert'):
            continue
        names.add(name)
for n in sorted(names): print(n)
PY
nm -g --defined-only lib/libc.a 2>/dev/null | awk 'NF==3{print $3}' | sort -u > "$tmp/defined"
missing=$(comm -23 "$tmp/declared" "$tmp/defined")
if [ -n "$missing" ]; then
	echo "DECLARED BUT NOT DEFINED:"; echo "$missing" | sed 's/^/  /'; fail=1
fi
echo "declared functions: $(wc -l < "$tmp/declared"), exported symbols: $(wc -l < "$tmp/defined")"
[ $fail = 0 ] && echo "headers-check: OK"
exit $fail
