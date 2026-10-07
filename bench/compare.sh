#!/bin/sh
# Join two benchmark outputs (lib-spfxd first, host second) into a table.
#   compare.sh spfxd.txt host.txt
# ratio = host / lib-spfxd: above 1.00 means lib-spfxd is faster.
awk '
	NR == FNR { s[$1] = $2; order[n++] = $1; next }
	{ h[$1] = $2 }
	END {
		printf "%-28s %12s %12s %8s\n", "benchmark (ns/op)", "lib-spfxd", "host", "ratio"
		for (i = 0; i < n; i++) {
			k = order[i]
			if (!(k in h)) continue
			r = s[k] > 0 ? h[k] / s[k] : 0
			printf "%-28s %12.2f %12.2f %8.2f\n", k, s[k], h[k], r
			if (r > 0) { lg += log(r); m++ }
		}
		if (m) printf "%-28s %12s %12s %8.2f\n", "geometric mean", "", "", exp(lg / m)
	}
' "$1" "$2"
