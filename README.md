# lib-spfxd

**lib-spfxd** (Standard Primitive Framework & eXtensions / Definitions) is a
complete C standard library for Linux on **x86-64** and **AArch64
(ARM64)**, written from first principles. It has its own public headers, raw system-call layer, program
startup objects, dynamic linker, allocator, stdio, math library, threads,
locales, time zones and networking. It does not use the host C library or
its headers, and it contains no code taken or ported from glibc, musl, the
BSD libcs, Newlib, uClibc, Bionic, dietlibc, Cosmopolitan or LLVM libc.

The host C library is used in exactly one place: the test suite, where it
serves as an *oracle* that lib-spfxd's output is compared against.

```
$ make -j"$(nproc)"                 # lib/libc.a, lib/libc.so, crt objects, lib/spfxd-gcc
$ lib/spfxd-gcc -O2 -o hello hello.c            # dynamically linked against lib-spfxd
$ lib/spfxd-gcc -static -O2 -o hello hello.c    # statically linked
$ make check                         # unit, oracle, dynamic-linking and libm accuracy tests
$ make bench                         # benchmarks against the host C library
$ make audit                         # nm/readelf/objdump/loader-trace audit of the outputs

$ make ARCH=aarch64 -j"$(nproc)"     # ARM64: lib-aarch64/ (cross-built on x86-64)
$ make ARCH=aarch64 check            # the same test suite, under qemu-aarch64
```

---

## Contents

1. [Building and using](#building-and-using)
2. [Repository layout](#repository-layout)
3. [What is implemented](#what-is-implemented)
4. [Design notes](#design-notes)
5. [Testing](#testing)
6. [Benchmarks](#benchmarks)
7. [Audit](#audit)
8. [AArch64 (ARM64) port](#aarch64-arm64-port)
9. [Unsupported features and documented differences](#unsupported-features-and-documented-differences)
10. [Self-audit checklist](#self-audit-checklist)
11. [License](#license)

---

## Building and using

Requirements: GCC (or a GCC-compatible compiler driver) targeting
x86-64 or AArch64 Linux, GNU make, binutils. Python 3 is needed only to regenerate
tables (`tools/gen-*.py`). The libm accuracy tests additionally use mpmath.

| Command | Result |
|---|---|
| `make` | `lib/libc.a`, `lib/libc.so`, `lib/crt1.o`, `lib/Scrt1.o`, `lib/rcrt1.o`, `lib/crti.o`, `lib/crtn.o`, empty `libm.a`/`libpthread.a`/… (everything lives in libc), and the compiler wrapper `lib/spfxd-gcc` with its specs file |
| `make check` | full test suite, static and dynamic (`tests/run.sh`) |
| `make bench` | benchmark lib-spfxd against the host C library (`bench/`) |
| `make audit` | build/ELF/dependency audit (`tools/audit.sh`) |
| `make headers-check` | every public header compiles on its own |
| `make install PREFIX=/opt/spfxd` | install headers, libraries and `bin/spfxd-gcc` |
| `make ARCH=aarch64 …` | any of the above for AArch64, into `build-aarch64/` and `lib-aarch64/` (see [AArch64 port](#aarch64-arm64-port)) |

`ARCH` defaults to the host's architecture; x86-64 builds go to `build/`
and `lib/`.

`lib/spfxd-gcc` passes everything through to the real compiler with a specs
file that replaces the host headers with lib-spfxd's (`-nostdinc`, then
`include/`), replaces the startup files and libraries, and sets the dynamic
linker to lib-spfxd's `libc.so`. The compiler's own include directory stays
on the search path *after* lib-spfxd's headers, so only compiler-specific
headers such as `<immintrin.h>` come from it. `libgcc` is linked as with
any C library.

```
lib/spfxd-gcc -O2 prog.c -o prog              # PIE, interpreter = lib/libc.so
lib/spfxd-gcc -static -O2 prog.c -o prog      # static (crt1.o)
lib/spfxd-gcc -static-pie -O2 prog.c -o prog  # static PIE (rcrt1.o, self-relocating)
lib/spfxd-gcc -shared -fPIC lib.c -o lib.so   # shared libraries
```

`libc.so` is its own dynamic linker: the kernel maps it as the program
interpreter, it relocates itself, loads the program's dependencies and
starts the program. Run directly it acts as a launcher and as `ldd`:

```
lib/libc.so ./prog args...                    # run prog with lib-spfxd's loader
lib/libc.so --list ./prog                     # list prog's dependencies
LD_TRACE_LOADED_OBJECTS=1 ./prog              # same, the ldd convention
```

Runtime environment variables: `LD_LIBRARY_PATH`, `LD_PRELOAD` (both ignored
for setuid/setgid programs), `TZ`, `TZDIR`, `LANG`/`LC_*`, and
`LIBSPFXD_CPU=baseline`, which disables the AVX2/FMA code paths (for testing
and diagnosis; also ignored for setuid programs).

---

## Repository layout

```
Makefile            build system (dependency tracking with -MMD)
include/            public headers (ISO C, POSIX, Linux)
arch/<arch>/        x86_64 and aarch64
  include/bits/     architecture parts of the public headers
  internal/         syscall/atomic/CPU-feature/relocation internals
  src/<dir>/        assembly and C that replaces src/<dir>/<same name>.c
src/<subsystem>/    portable implementation, one directory per area
ldso/               the dynamic linker (built into libc.so only)
crt/                crt1, Scrt1, rcrt1, crti, crtn
tests/              unit tests, host-oracle tests, dynamic-loading tests,
                    libm accuracy, exhaustive float and rounding-mode checks
bench/              benchmarks (one source built for both libraries)
tools/              table generators (mpmath), header check, audit,
                    compiler wrapper templates
```

About 35,000 lines of C and assembly and 8,800 lines of headers (both
architectures).

---

## What is implemented

**ISO C (C11/C17)**: all of `<assert.h>` `<complex.h>` `<ctype.h>`
`<errno.h>` `<fenv.h>` `<float.h>` `<inttypes.h>` `<iso646.h>` `<limits.h>`
`<locale.h>` `<math.h>` `<setjmp.h>` `<signal.h>` `<stdalign.h>`
`<stdarg.h>` `<stdatomic.h>` `<stdbool.h>` `<stddef.h>` `<stdint.h>`
`<stdio.h>` `<stdlib.h>` `<stdnoreturn.h>` `<string.h>` `<tgmath.h>`
`<threads.h>` `<time.h>` `<uchar.h>` `<wchar.h>` `<wctype.h>`.

**POSIX / X/Open**: `<aio.h>` `<arpa/inet.h>` `<cpio.h>` `<dirent.h>`
`<dlfcn.h>` `<fcntl.h>` `<fmtmsg.h>` `<fnmatch.h>` `<ftw.h>` `<glob.h>`
`<grp.h>` `<iconv.h>` `<langinfo.h>` `<libgen.h>` `<monetary.h>`
`<mqueue.h>` `<net/if.h>` `<netdb.h>` `<netinet/in.h>` `<netinet/tcp.h>`
`<nl_types.h>` `<poll.h>` `<pthread.h>` `<pwd.h>` `<regex.h>` `<sched.h>`
`<search.h>` `<semaphore.h>` `<spawn.h>` `<strings.h>` `<sys/ipc.h>`
`<sys/mman.h>` `<sys/msg.h>` `<sys/resource.h>` `<sys/select.h>`
`<sys/sem.h>` `<sys/shm.h>` `<sys/socket.h>` `<sys/stat.h>`
`<sys/statvfs.h>` `<sys/time.h>` `<sys/times.h>` `<sys/types.h>`
`<sys/uio.h>` `<sys/un.h>` `<sys/utsname.h>` `<sys/wait.h>` `<syslog.h>`
`<tar.h>` `<termios.h>` `<ulimit.h>` `<unistd.h>` `<utime.h>` `<utmpx.h>`
`<wordexp.h>`.

**Linux and common extensions**: `<sys/epoll.h>` `<sys/eventfd.h>`
`<sys/inotify.h>` `<sys/signalfd.h>` `<sys/timerfd.h>` `<sys/random.h>`
`<sys/prctl.h>` `<sys/mount.h>` `<sys/sendfile.h>` `<sys/statfs.h>`
`<sys/sysinfo.h>` `<sys/auxv.h>` `<sys/file.h>` `<sys/ioctl.h>`
`<sys/syscall.h>` `<sys/sysmacros.h>` `<sys/param.h>` `<elf.h>` `<link.h>`
`<endian.h>` `<byteswap.h>` `<err.h>` `<getopt.h>` `<malloc.h>`
`<alloca.h>` `<stdio_ext.h>` `<ucontext.h>` `<paths.h>`, plus the usual
GNU/BSD additions (`strlcpy`, `asprintf`, `getline`, `memmem`, `qsort_r`,
`reallocarray`, `explicit_bzero`, `getrandom`, `pthread_setname_np`, …).

`libc.so` exports 1,488 symbols on x86-64 (1,489 on AArch64, which adds
`__stack_chk_guard` and `__getauxval` for the ABI and libgcc). Everything (math, threads, real-time,
dynamic loading, sockets) lives in the one library; `libm.a`,
`libpthread.a`, `libdl.a`, `librt.a`, … are empty archives so existing build
lines keep working.

---

## Design notes

### System calls, startup, TLS
Inline-assembly `syscall` wrappers (`arch/x86_64/internal/syscall_arch.h`)
with errno translation in one place; cancellation points use a separate
entry (`__syscall_cp`) with a program-counter window so that "check, then
block" is race-free. `clock_gettime` (and with it `time` and `gettimeofday`)
goes through the vDSO when the kernel provides it. The thread control block sits at
`%fs:0` with the stack-protector canary at `%fs:0x28` (the x86-64 ABI
position GCC emits code for). Static executables lay out TLS for the
program; dynamic ones get static TLS for every module present at startup
and dynamic TLS (`__tls_get_addr`) for modules loaded with `dlopen`.

### Dynamic linker
`libc.so` relocates itself in two stages, then maps dependencies (search
order: `DT_RPATH` when no `DT_RUNPATH`, `LD_LIBRARY_PATH`, `DT_RUNPATH`,
system directories), resolves symbols through GNU or SysV hash tables,
handles copy relocations, `IRELATIVE`, TLS relocations and RELRO, runs
constructors in dependency order, and provides `dlopen`/`dlsym`/`dlclose`/
`dladdr`/`dl_iterate_phdr`, `r_debug` for debuggers, and
`_dl_find_object` for the unwinder. Requests for `libc.so.6`, `libm.so.6`,
`libpthread.so.0`, `ld-linux-x86-64.so.2` and similar names resolve to
lib-spfxd itself.

### Runtime CPU dispatch
`__init_cpu` (the first thing at startup) records AVX2 and FMA, reported
only when the OS saves the YMM state, plus ERMS and FSRM. Before that, and
with `LIBSPFXD_CPU=baseline`, every routine takes its SSE2 baseline path,
which every x86-64 CPU has.

### Strings
`memcpy`, `memmove`, `memset`, `strlen`, `strchr`/`strchrnul`, `memchr`,
`strcmp` and `memcmp` are hand-written assembly with SSE2 and AVX2
variants. They use aligned loads that never touch a page the operand does
not reach, 128 bytes per iteration in the AVX2 loops, overlapping head and
tail moves instead of byte loops, and `rep movsb` for large copies.
`strstr`/`memmem` use the two-way algorithm (linear time, constant space).

### Allocator ("spfxd-alloc")
Memory comes in 4 MiB-aligned segments. Small objects (≤ 32 KiB, 40 size
classes) live in spans of one class with no per-object header: `free`
finds the span with one AND and two table lookups. Large objects get whole
page runs that grow and shrink in place. Huge ones (> 2 MiB) get their own
mapping, resized with `mremap`. Per-thread caches serve the common path
with no atomics; batches move to and from per-class shared state under
per-class locks, and locks are elided while single-threaded. Hardening:
XOR-mangled, validated free-list links; segment magic; slot-boundary
checks (a multiply-shift exactness test); double-free detection in the
thread cache; aborts on invalid frees.

### stdio
Four function pointers per stream (read/write/seek/close) serve file
descriptors, memory streams (`fmemopen`, `open_memstream`,
`open_wmemstream`), `fopencookie` and `sprintf`/`sscanf` alike. `getc`/`putc`
are a pointer compare plus a load or store. printf formats integers from a
digit-pair table and floating point **exactly**: `%e %f %g %a` produce
correctly rounded output at any precision, in every rounding mode,
including `long double`. scanf implements every ISO conversion,
positional arguments, `%m` allocation and wide conversions, and follows
C11 7.21.6.2p9 for numeral prefixes.

### Numeric conversion
`strtod`/`strtof`/`strtold` are correctly rounded in every rounding mode.
They take Clinger's fast path when the significand and power of ten are
exact. Otherwise, for doubles in round-to-nearest, they use the
Eisel–Lemire algorithm (a 128-bit product with a generated table of powers
of five, which declines when the truncation could matter). Everything else
uses exact big-integer arithmetic over at most the number of digits that
can affect rounding. `strtol` and family check overflow without division.
`qsort` is a merge sort into a stack or heap buffer, which needs about
n log₂ n − n comparisons. When no buffer can be allocated it falls back to
an in-place introsort with ninther pivots and heapsort protection
(O(n log n) worst case).

### Math library
- **Accuracy.** Every double function is built on double-double kernels
  (error-free transformations, minimax polynomials and tables generated
  from first principles with mpmath by `tools/gen-math.py`). Arguments are
  reduced exactly: Cody–Waite for moderate inputs, a 256-bit Payne–Hanek
  window of 2/π for huge ones. Measured worst cases are ≤ 0.6 ulp for the
  elementary double functions, ≤ 2 ulp for x87 `long double` and ≤ 1.4 ulp
  for binary128 `long double` (AArch64). `float`
  results are the double result rounded once: correctly rounded except
  in extremely rare double-rounding cases. lib-spfxd is correctly rounded on inputs where
  glibc is off by one ulp (examples verified with mpmath: `asinh`, `sinh`,
  `cbrt`, `expm1`, `erfc`, `log1p`, `tgamma`).
- **Speed (Ziv fast paths).** `exp`, `exp2`, `exp10`, `log`, `log2`,
  `log10`, `pow` and `atan` first compute a cheap approximation hi + lo with
  a proven error bound ε (`src/math/fastpath.h` documents each bound). If
  hi + (lo − ε) and hi + (lo + ε) round to the same double, that is the
  correctly rounded result and it is returned; otherwise the accurate path
  runs. These functions are therefore correctly rounded in all four
  rounding modes, and the fast path never changes a result.
- **FMA clones.** Those functions, and the `float` fast paths below, also
  have a clone compiled for FMA3, selected at run time.
- **float.** `expf`, `exp2f`, `logf`, `log2f`, `log10f`, `powf`, `sinf`,
  `cosf`, `sincosf` and `tanf` compute in double with relative error below
  2^-49 and round to float unless the value lies within 64 double ulps of
  a float rounding boundary. The rest use the double function rounded
  once.
- **Other.** `fma`/`fmaf` use the FMA instruction when the CPU has it,
  and are otherwise computed exactly in software (256-bit integer
  arithmetic); `fmal` is always exact in software, in both `long double`
  formats. `<complex.h>` follows Kahan's branch-cut
  formulas. `<fenv.h>` drives both SSE (MXCSR) and x87 state.
  `math_errhandling` is `MATH_ERRNO | MATH_ERREXCEPT`.

### Threads
1:1 threads via `clone`. Each thread's stack, TLS and control block share
one mapping. Mutexes (normal, recursive, error-checking) and condition
variables are futex-based. The library also provides rwlocks, barriers,
spinlocks, semaphores (named and unnamed), `pthread_once`, TSD keys,
deferred and asynchronous cancellation, `pthread_kill`/`sigqueue`, C11
`<threads.h>`, and `<stdatomic.h>` via compiler builtins. Mutexes do not
spin before sleeping; on current x86 (where `pause` costs over 100 cycles)
and under virtualization, measurement showed spinning only delayed the
hand-off.

### Locales, wide characters, iconv
- **Locales.** The C/POSIX locale, plus a UTF-8 variant of it for
  `LC_CTYPE` (any name ending in `.UTF-8`/`.utf8`). Other categories behave
  as in the C locale.
- **Wide characters.** Multibyte conversion is strict RFC 3629 UTF-8.
  Wide-character classification and case mapping use Unicode tables
  generated by `tools/gen-unicode.py`.
- **iconv.** Converts between UTF-8, UTF-16, UTF-32 (with and without
  BOM), UCS-2/UCS-4, ASCII, ISO-8859-1 to -16, Windows-1250 to -1258,
  KOI8-R/U, CP437/850/866 and MacRoman. It supports `//TRANSLIT` and
  `//IGNORE`.

### Time
TZif v1–v4 files (including leap-second "right/" zones and the POSIX footer
rule) and POSIX TZ strings. Time zone names are tried as zoneinfo files
first, then as POSIX strings (the same order as glibc), and instants before
a zone's first transition use its time type 0 (LMT). `strftime`/`strptime`
implement all POSIX and common GNU conversions.

### Regular expressions, glob, wordexp
- **Regular expressions.** POSIX BRE/ERE (with the GNU `\+ \? \|` and word
  operators), compiled to a small instruction set. Without
  back-references a Pike VM matches in O(length × program) time with POSIX
  leftmost-longest semantics. With back-references a bounded backtracking
  engine is used, which reports `REG_ESPACE` instead of running
  exponentially.
- **wordexp.** Implements tilde, parameter and arithmetic expansion, field
  splitting and globbing; command substitution runs `/bin/sh`.

### Networking
A stub resolver reads `/etc/resolv.conf` (search, ndots, timeout,
attempts). It queries A and AAAA records in parallel over UDP, falling back
to TCP on truncation, and matches answers by id, question and source.
`/etc/hosts`, `/etc/services`, `/etc/protocols` and `/etc/networks` are
read directly. `getaddrinfo` orders results by RFC 6724 rules.
`getnameinfo`, `gethostbyname*`, `if_nameindex` and `inet_*` are included.

---

## Testing

`make check` runs `tests/run.sh`, which builds every test twice, static
and dynamic:

| Suite | What it checks |
|---|---|
| `tests/unit/*.c` (20 programs) | self-checking tests per subsystem: strings (including a fuzzer that covers every alignment, page-boundary cases with a `PROT_NONE` guard page, and multi-page operands), ctype, stdlib, stdio, scanf, malloc (hardening included), threads, signals, processes, file systems, time, locale, setjmp, fenv, startup, stack protector, network, IPC |
| baseline-CPU runs | the string tests again with `LIBSPFXD_CPU=baseline`, so the SSE2 paths stay covered on AVX2 machines |
| `tests/oracle/*.c` (11 programs) | built with the host compiler and C library and with lib-spfxd; outputs must be **byte-identical**: printf, strtod (plus 400,000 generated inputs aimed at the fast paths), math special cases (errno and exception flags included), the exactly specified `long double` functions (`fmal`, `sqrtl`, `rintl`, `fmodl`, `remquol`, … on random bit patterns in all four rounding modes), time zones (20 zones, 1900–2100), regex, iconv, netdb, strfmon, wordexp |
| `tests/dynamic/` | shared-library dependency chains, `dlopen`/`dlsym`/`dlclose`, dynamic TLS, constructors and destructors |
| `tests/math/ulp_check.py` | mpmath-verified accuracy of libm (86 function/range cases; the `long double` format is taken from the build) |
| `tests/math/judge.py` | correct rounding of `exp exp2 log log2 log10 pow atan` in all four rounding modes (mpmath) |

Each architecture runs 69 test programs/cases; both currently pass 69/69
(AArch64 under qemu-aarch64 user-mode emulation, see below).

Additional verification run during development (not part of `make check`
because of its run time):

* `tests/math/float_exhaustive.c` compared every one of the 2^32 inputs of
  `expf exp2f logf log2f log10f sinf cosf tanf` with the double function
  rounded once, on both the FMA and the baseline code paths: 0
  mismatches.
* `tests/math/mdump.c` dumps 2 million arguments and results per function.
  Every libm optimization was checked bit for bit against the dump of the
  previous accurate implementation, with differences judged by mpmath.
  Those differences were all fixes of previously wrong subnormal results.

Where the oracle comparison excludes something, it is documented in the
test itself. One example is glibc's `%#g` of 999999.5, which glibc gets
wrong. Another is glibc evaluating POSIX TZ rules before 1970 with the 1970
transition dates, which lib-spfxd does not do.

---

## Benchmarks

`make bench` builds `bench/bench.c` against both libraries and prints
nanoseconds per operation, best of 7 runs. Measured on an Intel Xeon
(Sapphire Rapids class, 2.1 GHz, 4 vCPUs, shared cloud VM, so timings
fluctuate by about ±10%) against the host glibc. Both binaries are
dynamically linked. A ratio above 1 means lib-spfxd is faster.

| Benchmark (ns/op) | lib-spfxd | glibc | ratio |
|---|---:|---:|---:|
| memcpy/16 | 2.31 | 3.33 | 1.44 |
| memcpy/256 | 4.57 | 3.36 | 0.74 |
| memcpy/4096 | 35.37 | 36.18 | 1.02 |
| memcpy/1M | 38619.62 | 39867.11 | 1.03 |
| memmove/4096 | 41.30 | 38.69 | 0.94 |
| memset/64 | 2.65 | 2.52 | 0.95 |
| memset/4096 | 25.92 | 26.29 | 1.01 |
| strlen/16 | 3.52 | 2.98 | 0.85 |
| strlen/1024 | 10.92 | 12.51 | 1.15 |
| strchr/1024 | 14.51 | 11.52 | 0.79 |
| memchr/1024 | 9.99 | 9.49 | 0.95 |
| strcmp/1024 | 18.70 | 14.81 | 0.79 |
| memcmp/1024 | 12.51 | 13.02 | 1.04 |
| strstr/4096 | 55.74 | 77.46 | 1.39 |
| malloc+free/32 | 8.76 | 8.68 | 0.99 |
| malloc+free/512 | 8.38 | 8.77 | 1.05 |
| malloc+free/64K | 27.99 | 21.81 | 0.78 |
| malloc/mixed | 13.77 | 20.43 | 1.48 |
| realloc/grow | 177.04 | 304.69 | 1.72 |
| malloc/4threads | 3.32 | 2.79 | 0.84 |
| snprintf/int | 114.79 | 110.26 | 0.96 |
| snprintf/str | 136.99 | 85.13 | 0.62 |
| snprintf/%g | 124.09 | 257.29 | 2.07 |
| snprintf/%.17g | 284.19 | 510.41 | 1.80 |
| strtod | 44.36 | 99.59 | 2.25 |
| strtol | 14.23 | 17.66 | 1.24 |
| sscanf | 165.42 | 129.56 | 0.78 |
| fputs/devnull | 22.21 | 22.91 | 1.03 |
| fprintf/devnull | 103.07 | 109.42 | 1.06 |
| qsort/100 | 5507.90 | 5295.12 | 0.96 |
| qsort/100000 | 12307110.60 | 12343066.65 | 1.00 |
| sin | 18.38 | 8.72 | 0.47 |
| cos | 18.40 | 15.21 | 0.83 |
| tan | 30.30 | 14.79 | 0.49 |
| exp | 7.07 | 8.62 | 1.22 |
| log | 8.20 | 4.86 | 0.59 |
| log2 | 9.01 | 5.20 | 0.58 |
| pow | 27.48 | 15.02 | 0.55 |
| atan | 22.67 | 7.67 | 0.34 |
| sqrt | 1.86 | 1.88 | 1.01 |
| cbrt | 20.73 | 17.05 | 0.82 |
| erf | 43.49 | 13.25 | 0.30 |
| tgamma | 100.54 | 61.97 | 0.62 |
| sinf | 15.38 | 4.75 | 0.31 |
| mutex/uncontended | 17.74 | 19.52 | 1.10 |
| mutex/4threads | 65.69 | 52.46 | 0.80 |
| pthread_create+join | 42758.53 | 42620.68 | 1.00 |
| getenv | 126.45 | 107.02 | 0.85 |
| localtime_r | 258.10 | 513.96 | 1.99 |
| **geometric mean** | | | **0.91** |

Reading the numbers:
- **Ahead of glibc.** `strtod` (2.3×, Eisel–Lemire), floating-point printing
  (1.8–2.1×, exact digit generation is cheaper than glibc's multi-precision
  path), `localtime_r` (2×), `realloc` growth and mixed allocation
  (1.5–1.7×, thread caches and in-place growth), `strstr` (two-way) and
  `strtol`.
- **About even.** The AVX2 string routines, memcpy/memset, malloc/free
  pairs, `qsort`, thread creation, `fprintf`, integer `snprintf` and the
  uncontended mutex. Several individual string rows move by ±20% between
  runs on this shared VM.
- **Behind glibc.** Most libm functions, by design. Results are correctly
  rounded (or within 0.6 ulp) through an error-bounded fast path plus a
  rounding test, while glibc returns a slightly less accurate result
  directly. `exp` is now level with glibc. `tan`, `erf`, `tgamma`, `atan`
  and `sinf` remain 2–3× slower. Also behind: `snprintf` with string
  padding, `sscanf`, contended mutexes, and 64 KiB malloc/free (lock
  cost).

---

## Audit

`make audit` (`tools/audit.sh`) rebuilds from scratch and checks:

* the build is free of compiler and assembler warnings;
* `libc.so`: no `DT_NEEDED`, no undefined dynamic symbols, no `TEXTREL`,
  `BIND_NOW` with RELRO, non-executable stack, no glibc symbol versions,
  and no internal helpers among the exported `__` names;
* `libc.a`: every undefined reference is satisfied inside the archive,
  except the linker's `_GLOBAL_OFFSET_TABLE_` and libgcc helpers;
* the crt objects reference only `main`, `__libc_start_main` and
  linker-defined symbols, and all mark a non-executable stack;
* a static test program has no interpreter or dynamic section. A dynamic
  one names `lib/libc.so` as interpreter and `libc.so` as its only
  `DT_NEEDED`, and the loader trace (`LD_TRACE_LOADED_OBJECTS`, the `ldd`
  mechanism) lists nothing else. Both programs run correctly;
* headers never reach for host headers, and each compiles on its own;
* the source tree contains no copyright notices of other C libraries.

Current result: 35 checks passed, 0 failed, for both `make audit` and
`make ARCH=aarch64 audit` (which uses the cross binutils and runs the test
programs, and the loader trace, under qemu).

---

## AArch64 (ARM64) port

lib-spfxd builds for AArch64 Linux from the same sources: the portable
code in `src/` is shared, and `arch/aarch64/` holds what differs.

```
make ARCH=aarch64 -j"$(nproc)"          # cross: needs aarch64-linux-gnu-gcc
lib-aarch64/spfxd-gcc -O2 prog.c -o prog
qemu-aarch64 ./prog                     # on an x86-64 host
make ARCH=aarch64 check                 # tests under qemu-aarch64
make ARCH=aarch64 audit
```

On an ARM64 Linux machine (including a Linux VM or container on Apple
Silicon), plain `make`, `make check` and `make audit` build and test
natively.

**Apple Silicon / macOS.** lib-spfxd cannot replace the C library of macOS
itself: macOS has no stable system-call ABI (only `libSystem` is
supported, and Apple changes the raw interface between releases), every
process must be started by `dyld` and link `libSystem`, and executables
are Mach-O rather than ELF. On a Mac, use lib-spfxd inside an ARM64 Linux
VM or container (for example Docker Desktop, UTM, Lima or OrbStack); there
it runs natively at full speed.

What the port contains:

| Area | AArch64 implementation |
|---|---|
| System calls | `svc #0` with the number in `x8` (`arch/aarch64/internal/syscall_arch.h`); the table is generated from the kernel's `asm/unistd.h` by `tools/gen-syscall.sh`. The generic table has no `open`, `stat`, `poll`, `dup2`, `pause` or `fork`; the portable code uses `openat`, `fstatat`, `ppoll`, `dup3` and `clone` everywhere. vDSO: `__kernel_clock_gettime`, `__kernel_gettimeofday`, `__kernel_clock_getres`. |
| Startup | `_start`, `crti`/`crtn` and the dynamic linker's entry in assembly (`arch/aarch64/internal/crt_arch.h`, `arch/aarch64/crt/`). |
| TLS | Variant I: the thread pointer (`TPIDR_EL0`) points just past `struct pthread`, followed by a 16-byte TCB and the TLS blocks (`TLS_ABOVE_TP`). The stack-protector canary is the global `__stack_chk_guard`, as the AArch64 compiler expects. |
| Threads, signals | `clone` wrapper, cancellable-syscall window, `rt_sigreturn` trampoline, `vfork` in assembly. |
| setjmp / ucontext | x19–x30, sp and d8–d15 (plus the signal mask for `sigsetjmp`); `getcontext`/`setcontext`/`swapcontext`/`makecontext` against the kernel's `struct sigcontext` (FP/SIMD state in the `fpsimd_context` record). |
| fenv | FPCR (rounding, trap enables) and FPSR (flags); `feenableexcept` reports failure on cores without trapping support, as the hardware does. |
| Dynamic linker | `R_AARCH64_*` relocations, including TLS descriptors (`R_AARCH64_TLSDESC`, GCC's default dialect on AArch64): a static resolver for modules present at startup and a register-preserving dynamic resolver for `dlopen`ed ones (`arch/aarch64/src/ldso/tlsdesc.S`). |
| Atomics | The library is built with `-mno-outline-atomics` (LSE or LL/SC inline). Programs built with GCC's default outline atomics work too: libgcc's helpers find `__getauxval`. |
| `long double` | IEEE binary128 (113-bit significand), implemented in software (see below). |
| Strings, math | The portable C string routines (no assembly yet); the double/float libm is shared, with `fsqrt`, `fmadd` and `frintx` used directly. |

**binary128 `long double`.** printf and strtold handle the 113-bit
significand exactly (the formatting and Eisel-Lemire/big-number paths
work with 128-bit integers). The `long double` math library
(`src/math/ldbl128.c`) is new code for the format: exact functions
(rounding to integer, `frexpl`/`scalbnl`/`nextafterl`, `fmodl`/`remquol`
by integer long division, `sqrtl` digit by digit and correctly rounded in
the current mode, `fmal` with a 256-bit accumulator) work on the bit
pattern; trigonometric reduction is an integer Payne–Hanek product with a
448-bit window of 2/π; `exp`/`log`/`pow`/`atan` and relatives use
table-driven reduction with double-binary128 leading terms. Tables and
Taylor coefficients come from `tools/gen-ldbl128.py` (mpmath). The
hardware has no binary128 arithmetic, so every operation is a libgcc
soft-float call (tens of instructions); these functions are accurate but
much slower than their double counterparts.

**Testing under emulation.** On an x86-64 host the tests run under
`qemu-aarch64` (user mode); the oracle's reference programs are built with
`aarch64-linux-gnu-gcc` and run against the cross toolchain's glibc. Three
`process` checks that exec a program by path (`execl`, `execle`, and
`posix_spawn` of a missing file) are skipped when `SPFXD_EMULATED` is set,
because qemu's user-mode `execve` behaves differently there; glibc fails the
same three checks under qemu. The iconv oracle takes its reference from
the native glibc, since the cross toolchain's glibc has no converter
modules (`ORACLE_PORTABLE`). Benchmarks are not reported for AArch64:
timings under an emulator say nothing about real hardware.

---

## Unsupported features and documented differences

Nothing below is a silent stub: unsupported operations fail with the
documented error.

| Area | Behaviour |
|---|---|
| Message catalogs | `catopen` always fails with `ENOENT`; `catgets` returns the default string. |
| Locales | Only C/POSIX and the UTF-8 `LC_CTYPE` variant. Other locale names are rejected by `setlocale`. `LC_COLLATE`, `LC_MONETARY`, `LC_NUMERIC`, `LC_TIME` and `LC_MESSAGES` always behave as in the C locale. Wide-character classification is Unicode in every locale. |
| iconv | No multi-byte East Asian encodings (Shift_JIS, EUC-*, GB*, Big5): `iconv_open` fails with `EINVAL`. BOM-less UTF-16/32 input is read in host byte order. |
| Mutexes | Robust mutexes and the priority-inheritance and priority-protection protocols: the attribute setters return `ENOTSUP`. `PTHREAD_SCOPE_PROCESS`: `ENOTSUP`. |
| Dynamic linking | On x86-64, TLS descriptors (`R_X86_64_TLSDESC`, `-mtls-dialect=gnu2`) are not supported and are reported as an unsupported relocation (AArch64 supports them). `dlclose` never unmaps a library (constructors' state stays valid). `dlopen` in statically linked programs fails with a `dlerror` message. |
| aio | Each request runs on its own thread. Requests on one descriptor may complete in any order, as POSIX allows. In-progress requests cannot be cancelled (`AIO_NOTCANCELED`). |
| getaddrinfo | Returns no `SOCK_RAW` entries. Numeric ports above 65535 are rejected. |
| scanf | Follows ISO C for numeral prefixes: `"1.5e"` is a matching failure (glibc accepts it). |
| strtol | Follows C17: no `0b` binary prefix (C23 adds it; recent glibc accepts it under `_GNU_SOURCE`). |
| qsort | Merge sort (stable in practice) with a temporary buffer; in-place introsort when the buffer cannot be allocated. Stability is not guaranteed by the interface. |
| utmpx | Uses the system's record layout with 32-bit times (as the files on disk are written). |
| long double libm | `erfl`, `erfcl`, `lgammal`, `tgammal` and the Bessel functions are computed in double precision (both formats). On AArch64, binary128 arithmetic is software-emulated and slow. |
| Architectures | x86-64 and AArch64 Linux only. macOS (including Apple Silicon) cannot be targeted: see [AArch64 port](#aarch64-arm64-port). AArch64 string functions are portable C, without assembly. |
| Accuracy limits | Bessel functions and `lgamma` for negative arguments lose relative accuracy close to their zeros. |
| Rounding modes | The Ziv-path functions are correctly rounded in all modes. Other libm functions are designed for round-to-nearest; in directed modes they stay within about one ulp. |
| NaN sign | Invalid operations return the hardware's default NaN: sign bit set on x86, clear on AArch64 (as glibc does on each). The sign of a NaN is unspecified by C. |

---

## Self-audit checklist

| Requirement | Status |
|---|---|
| Written from first principles, no code from other C libraries | Yes. Tables and polynomials are generated by the scripts in `tools/` from mpmath; the audit scans for foreign copyright notices. |
| No dependency on the host C library or its headers | Yes. `-nostdinc -ffreestanding` build; the audit verifies no `DT_NEEDED`, no glibc versions, and no undefined references outside the archive. |
| Own public headers, raw syscall layer, crt objects | Yes: `include/`, `arch/<arch>/internal/syscall_arch.h`, `crt/`. |
| ARM64 support | AArch64 Linux port (cross-built, tested under qemu; native on ARM64 Linux). macOS is not a possible target; see the port section. |
| `libc.a`, `libc.so`, `crt1.o`, `crti.o`, `crtn.o` (plus `Scrt1.o`, `rcrt1.o`) | Built by `make`. |
| Full ISO C; substantial POSIX and Linux | Yes; see [What is implemented](#what-is-implemented). |
| Own allocator | spfxd-alloc (`src/memory/`). |
| Optimized string routines (assembly) | x86-64: SSE2 and AVX2, runtime dispatch (`arch/x86_64/src/string/`). AArch64: portable C. |
| stdio with printf/scanf | Exact floating-point formatting, full scanf (`src/stdio/`). |
| Robust strto* | Correctly rounded in all rounding modes; overflow-safe integer parsing. |
| qsort | Merge sort with an in-place introsort fallback; O(n log n) worst case. |
| Time zones | TZif v1–v4, leap seconds, POSIX rules. |
| Accurate libm with fenv | Double-double kernels; correctly rounded fast paths; full `<fenv.h>`. |
| setjmp, signals | Yes, including `sigsetjmp`/`siglongjmp` and `ucontext`. |
| pthreads with TLS, atomics | Yes: static and dynamic TLS, `<stdatomic.h>`, C11 threads. |
| Locales / UTF-8, wide characters | C/POSIX and UTF-8 `LC_CTYPE`; Unicode classification tables. |
| Automated tests (host libc only as oracle) | `make check`: 69/69 on x86-64; `make ARCH=aarch64 check`: 69/69 under qemu. |
| Benchmarks | `make bench`; results above. |
| Audit with nm/readelf/objdump/ldd | `make audit` and `make ARCH=aarch64 audit`: 35/35 checks pass for each. |
| Warnings fixed | The audit's full rebuild has no warnings. |
| No fake stubs; unsupported parts documented | See the table above. |

---

## License

MIT; see [LICENSE](LICENSE).
