#!/usr/bin/env python3
"""Judge mdump output against mpmath.

    judge.py <function> <mode: near|up|down|zero> < dump

For each line "<x> <y> <r>" (hex bit patterns) computes the exact value
with mpmath and reports results that are not the correctly rounded value
in the given rounding mode, with their distance in ulps.  Exit status 1 if
any result is off by a full ulp or more (rounding-mode bugs), 0 otherwise;
results within one ulp that are not correctly rounded are counted
separately (the accurate paths promise < 0.51 ulp to nearest only).
"""
import struct, sys
import mpmath as mp
mp.mp.prec = 200

fn, mode = sys.argv[1], sys.argv[2]
dbl = not fn.endswith("f") or fn in ("erf",)
def d(h): return struct.unpack("<d", struct.pack("<Q", int(h, 16)))[0]
def f(h): return struct.unpack("<f", struct.pack("<I", int(h, 16)))[0]
conv = d if len(sys.stdin.buffer.peek(20).split()[0]) == 16 else f

F = {"exp": mp.exp, "exp2": lambda x: mp.power(2, x), "log": mp.log, "log2": lambda x: mp.log(x, 2),
     "log10": mp.log10, "log1p": mp.log1p, "sin": mp.sin, "cos": mp.cos, "tan": mp.tan, "atan": mp.atan,
     "pow": mp.power}
base = fn[:-1] if fn not in F and fn.endswith("f") else fn
func = F[base]
prec, emin = (53, -1074) if conv is d else (24, -149)

def rnd(v):
    """v (mpf) rounded to the target format in `mode`."""
    if v == 0: return mp.mpf(0)
    s = -1 if v < 0 else 1
    a = abs(v)
    e = mp.floor(mp.log(a, 2))
    q = max(e - (prec - 1), emin)
    m = a / mp.mpf(2) ** q
    fl = mp.floor(m)
    if mode == "near":
        r = mp.nint(m)
    elif mode == "zero":
        r = fl
    elif mode == "up":
        r = fl if s < 0 else mp.ceil(m)
    else:  # down
        r = mp.ceil(m) if s < 0 else fl
    return s * r * mp.mpf(2) ** q

bad = near = n = 0
for line in sys.stdin:
    xs, ys, rs = line.split()
    x, y, r = conv(xs), conv(ys), conv(rs)
    if r != r or r in (float("inf"), float("-inf")) or r == 0: continue
    v = func(mp.mpf(x), mp.mpf(y)) if base == "pow" else func(mp.mpf(x))
    if abs(v) >= (mp.mpf(2) ** (1024 if conv is d else 128)) * (1 - mp.mpf(2) ** -(prec + 1)):
        continue                      # overflow: inf or the largest finite, by mode
    want = rnd(v)
    n += 1
    if mp.mpf(r) != want:
        ulp = abs(mp.mpf(r) - want) / (mp.mpf(2) ** (mp.floor(mp.log(abs(want), 2)) - (prec - 1)))
        if ulp >= 1:
            bad += 1
            if bad <= 5: print("%s(%r%s) [%s] = %r, want %s (%.2f ulp)" % (fn, x, ", %r" % y if base == "pow" else "", mode, r, mp.nstr(want, 20), float(ulp)))
        else:
            near += 1
print("%s %s: %d checked, %d not correctly rounded, %d off by >= 1 ulp" % (fn, mode, n, near + bad, bad))
sys.exit(1 if bad else 0)
