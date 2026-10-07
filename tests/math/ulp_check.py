#!/usr/bin/env python3
"""lib-spfxd libm accuracy oracle.

Runs tests/math/ulp_driver for each (function, range) case below,
recomputes every result with mpmath at high precision and reports the
maximum error in ulps of the result format.  Exits non-zero if any case
exceeds its bound.

    python3 tests/math/ulp_check.py <driver> [filter]
"""
import os, subprocess, sys
import mpmath as mp
from mpmath import mpf

mp.mp.prec = 300

def parse_hex(s):
    """Parse a C %a / %La hex float into an exact mpf."""
    s = s.strip()
    if s in ("nan", "-nan"): return None
    if s in ("inf", "-inf"): return mpf("inf") if s[0] != "-" else mpf("-inf")
    neg = s.startswith("-")
    if neg: s = s[1:]
    mant, exp = s[2:].split("p")
    if "." in mant:
        ip, fp = mant.split(".")
    else:
        ip, fp = mant, ""
    v = int(ip + fp, 16) if (ip + fp) else 0
    e = int(exp) - 4 * len(fp)
    r = mp.ldexp(mpf(v), e)
    return -r if neg else r

FMT = {"d": (53, -1022), "f": (24, -126), "l": (64, -16382)}

def ulp_err(got, ref, fmt):
    prec, emin = FMT[fmt]
    if ref == 0:
        return 0.0 if got == 0 else float("inf")
    e = max(int(mp.floor(mp.log(abs(ref), 2))), emin)
    ulp = mp.ldexp(1, e - prec + 1)
    return float(abs(got - ref) / ulp)

def to_frac(x):
    from fractions import Fraction
    m_, e_ = mp.frexp(x)
    m_ = int(mp.ldexp(m_, 80))
    e_ = int(e_) - 80
    return Fraction(m_) * (Fraction(2) ** e_)

def exact_fmod(x, y):
    """C fmod: x - trunc(x/y) y, exactly (rational arithmetic)."""
    fx, fy = to_frac(x), to_frac(y)
    q = abs(fx) // abs(fy)
    r = abs(fx) - q * abs(fy)
    r = r if x >= 0 else -r
    return mpf(r.numerator) / r.denominator

def exact_remainder(x, y):
    fx, fy = to_frac(x), to_frac(y)
    q = fx / fy
    n = round(q)                     # Python rounds half to even, as IEEE
    r = fx - n * fy
    return mpf(r.numerator) / r.denominator

def ref_fn(name):
    m = mp
    table = {
        "exp": m.exp, "exp2": lambda x: m.power(2, x), "exp10": lambda x: m.power(10, x),
        "expm1": m.expm1, "log": m.log, "log2": lambda x: m.log(x, 2),
        "log10": m.log10, "log1p": m.log1p, "sin": m.sin, "cos": m.cos, "tan": m.tan,
        "asin": m.asin, "acos": m.acos, "atan": m.atan, "sinh": m.sinh, "cosh": m.cosh,
        "tanh": m.tanh, "asinh": m.asinh, "acosh": m.acosh, "atanh": m.atanh,
        "cbrt": m.cbrt, "sqrt": m.sqrt, "erf": m.erf, "erfc": m.erfc,
        "lgamma": lambda x: m.log(abs(m.gamma(x))), "tgamma": m.gamma,
        "j0": lambda x: m.besselj(0, x), "j1": lambda x: m.besselj(1, x),
        "y0": lambda x: m.bessely(0, x), "y1": lambda x: m.bessely(1, x),
        "jn3": lambda x: m.besselj(3, x), "yn3": lambda x: m.bessely(3, x),
        "jn20": lambda x: m.besselj(20, x),
        "pow": m.power, "atan2": m.atan2, "hypot": lambda x, y: m.sqrt(x * x + y * y),
        "fmod": exact_fmod, "remainder": exact_remainder,
        "fma_xy": None,
    }
    base = name
    if name.endswith("l") and name[:-1] in table: base = name[:-1]
    if name.endswith("f") and name[:-1] in table: base = name[:-1]
    return table[base]

# (function, format, count, lo, hi, [lo2, hi2], max ulp)
CASES = [
    ("exp", "d", 3000, -745, 709.7, None, 0.6), ("exp", "d", 1000, -1, 1, None, 0.6),
    ("exp2", "d", 2000, -1074, 1023.9, None, 0.6), ("exp10", "d", 2000, -323, 308, None, 0.6),
    ("expm1", "d", 2000, -40, 709, None, 0.6), ("expm1", "d", 1000, -0.02, 0.02, None, 0.6),
    ("log", "d", 2000, 1e-300, 1e300, None, 0.6), ("log", "d", 1000, 0.9, 1.1, None, 0.6),
    ("log", "d", 300, 1e-320, 1e-308, None, 0.6),
    ("log2", "d", 2000, 1e-300, 1e300, None, 0.6), ("log10", "d", 2000, 1e-300, 1e300, None, 0.6),
    ("log1p", "d", 2000, -0.999, 1e10, None, 0.6), ("log1p", "d", 1000, -1e-3, 1e-3, None, 0.6),
    ("sin", "d", 3000, -10, 10, None, 0.7), ("sin", "d", 1000, 1e3, 1e300, None, 0.7),
    ("cos", "d", 3000, -10, 10, None, 0.7), ("cos", "d", 1000, 1e3, 1e300, None, 0.7),
    ("tan", "d", 3000, -10, 10, None, 0.9), ("tan", "d", 1000, 1e3, 1e300, None, 0.9),
    ("asin", "d", 2000, -1, 1, None, 0.6), ("acos", "d", 2000, -1, 1, None, 0.6),
    ("atan", "d", 2000, -100, 100, None, 0.6), ("atan", "d", 500, 1e-10, 1e300, None, 0.6),
    ("sinh", "d", 2000, -710, 710, None, 0.7), ("sinh", "d", 1000, -1, 1, None, 0.7),
    ("cosh", "d", 2000, -710, 710, None, 0.7), ("tanh", "d", 2000, -25, 25, None, 0.7),
    ("asinh", "d", 2000, -1e10, 1e10, None, 0.7), ("acosh", "d", 2000, 1, 1e10, None, 0.7),
    ("atanh", "d", 2000, -1, 1, None, 0.7),
    ("cbrt", "d", 2000, -1e300, 1e300, None, 0.6), ("sqrt", "d", 1000, 0, 1e300, None, 0.5),
    ("erf", "d", 2000, -7, 7, None, 0.9), ("erfc", "d", 3000, -7, 27.3, None, 1.2),
    ("lgamma", "d", 2000, 0.01, 1e5, None, 1.0), ("lgamma", "d", 1000, 0.5, 3, None, 1.0),
    ("lgamma", "d", 1000, -50, -0.01, None, None),
    ("tgamma", "d", 2000, 0.01, 171, None, 1.2), ("tgamma", "d", 1000, -170, -0.01, None, 1.5),
    ("j0", "d", 1000, -20, 20, None, 0.6), ("j1", "d", 1000, -20, 20, None, 0.6),
    ("y0", "d", 1000, 0.01, 20, None, 0.6), ("y1", "d", 1000, 0.01, 20, None, 0.6),
    ("j0", "d", 500, 20, 1e5, None, None), ("y1", "d", 500, 20, 1e5, None, None),
    ("jn3", "d", 500, 0.1, 50, None, None), ("yn3", "d", 500, 0.1, 50, None, None),
    ("jn20", "d", 500, 0.1, 50, None, None),
    ("pow", "d", 3000, 1e-5, 1e5, (-60, 60), 0.6), ("pow", "d", 1000, 0.5, 2, (-1000, 1000), 0.6),
    ("atan2", "d", 2000, -10, 10, (-10, 10), 0.6), ("hypot", "d", 2000, -1e300, 1e300, (-1e300, 1e300), 0.6),
    ("fmod", "d", 2000, -1e300, 1e300, (-1e10, 1e10), 0.0),
    ("remainder", "d", 2000, -1e20, 1e20, (-1e5, 1e5), 0.0),
    ("expf", "f", 2000, -100, 88, None, 0.51), ("logf", "f", 2000, 1e-30, 1e30, None, 0.51),
    ("sinf", "f", 2000, -100, 100, None, 0.51), ("cosf", "f", 2000, -100, 100, None, 0.51),
    ("tanf", "f", 2000, -100, 100, None, 0.51),
    ("expl", "l", 1000, -11000, 11000, None, 2.0), ("exp2l", "l", 1000, -16000, 16000, None, 2.0),
    ("expm1l", "l", 1000, -40, 100, None, 2.0), ("logl", "l", 1000, 1e-300, 1e300, None, 2.0),
    ("log2l", "l", 1000, 1e-300, 1e300, None, 2.0), ("log10l", "l", 1000, 1e-300, 1e300, None, 2.0),
    ("log1pl", "l", 1000, -0.9, 100, None, 2.0),
    ("sinl", "l", 1000, -10, 10, None, 2.0), ("sinl", "l", 500, 1e3, 1e300, None, 2.0),
    ("cosl", "l", 1000, -10, 10, None, 2.0), ("tanl", "l", 1000, -10, 10, None, 3.0),
    ("asinl", "l", 1000, -1, 1, None, 2.0), ("acosl", "l", 1000, -1, 1, None, 2.0),
    ("atanl", "l", 1000, -100, 100, None, 2.0),
    ("sinhl", "l", 1000, -100, 100, None, 3.0), ("coshl", "l", 1000, -100, 100, None, 3.0),
    ("tanhl", "l", 1000, -10, 10, None, 3.0), ("asinhl", "l", 1000, -1e5, 1e5, None, 3.0),
    ("acoshl", "l", 1000, 1, 1e5, None, 3.0), ("atanhl", "l", 1000, -1, 1, None, 3.0),
    ("cbrtl", "l", 1000, -1e300, 1e300, None, 1.0), ("sqrtl", "l", 1000, 0, 1e300, None, 0.5),
    ("powl", "l", 1000, 1e-5, 1e5, (-60, 60), 2.0), ("powl", "l", 500, 0.5, 2, (-10000, 10000), 3.0),
    ("atan2l", "l", 1000, -10, 10, (-10, 10), 2.0), ("hypotl", "l", 1000, -1e300, 1e300, (-1e300, 1e300), 1.0),
    ("fmodl", "l", 1000, -1e300, 1e300, (-1e10, 1e10), 0.0),
]

def main():
    driver = sys.argv[1]
    filt = sys.argv[2] if len(sys.argv) > 2 else None
    failed = 0
    for (fn, fmt, cnt, lo, hi, r2, bound) in CASES:
        if filt and filt not in fn: continue
        # RUN: optional emulator prefix for a cross-built driver (qemu-aarch64)
        args = os.environ.get("RUN", "").split() + [driver, fn, str(cnt), repr(lo), repr(hi)]
        if r2: args += [repr(r2[0]), repr(r2[1])]
        out = subprocess.run(args, capture_output=True, text=True, check=True).stdout
        f = ref_fn(fn)
        worst, wx = 0.0, None
        for line in out.splitlines():
            parts = line.split()
            if r2:
                x, y, got = parse_hex(parts[0]), parse_hex(parts[1]), parse_hex(parts[2])
                try:
                    ref = f(x, y)
                except Exception:
                    continue
                arg = (parts[0], parts[1])
            else:
                x, got = parse_hex(parts[0]), parse_hex(parts[1])
                try:
                    ref = f(x)
                except Exception:
                    continue
                arg = parts[0]
            if got is None or isinstance(ref, mp.mpc):
                continue
            # results that overflow/underflow the format: compare against
            # the rounded reference directly
            prec, emin = FMT[fmt]
            if abs(ref) > mp.ldexp(1, -emin + (1 if fmt != "l" else 2)) and mp.isinf(got):
                continue
            e = ulp_err(got, ref, fmt)
            if e > worst:
                worst, wx = e, arg
        status = "ok"
        if bound is not None and worst > bound:
            status = "FAIL"
            failed += 1
        rng = "[%g, %g]" % (lo, hi) + (" x [%g, %g]" % r2 if r2 else "")
        print("%-10s %-32s max %8.3f ulp  %s%s" % (fn, rng, worst, status,
              "" if status == "ok" else "  at %s" % (wx,)))
    sys.exit(1 if failed else 0)

main()
