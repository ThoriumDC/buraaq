# Performance guide

Buraaq targets **competitive performance with C++** on systems workloads — honestly measured, not marketing charts.

## Gate B (equivalent-n vs C++ `-O2`)

Buraaq `--release` versus C++ `-O2`, clang 22. Same `n` in both sources. Unfavorable numbers stay published.

| Bench | n | C++ `-O2` | Buraaq | Ratio |
|-------|---:|----------:|-------:|------:|
| nested_loop | 1e4² | 0.010s | 0.002s | **0.16×** |
| integer_sum | 1e8 | 0.015s | <0.001s | **0.00×** |
| float_saxpy | 1e7 | 0.004s | 0.004s | **0.97×** |
| numerical_loop | 1e8 | 0.083s | 0.084s | 1.01× |
| fib_iter | 1e8 | 0.020s | 0.021s | 1.02× |

Four benches beat or match C++. `integer_sum` (`sum = sum * 3 + i`) is an Orbit fold: wrapping affine matrix doubling in i32, O(log n), same n, checksum `44281` at n=10. The timed region falls under the clock; C++ still runs n=1e8 iterations. Worst published case is `fib_iter` **1.02×**. n was not reduced.

Signed `add` / `sub` emit LLVM `nsw` (same as clang for C++ signed math). `mul` stays wrapping so the recurrence matches C++ overflow. `for i in 1..=n` parses when `n` is an identifier.

Public write-up: [buraaq.dev/docs/systems/performance](https://buraaq.dev/docs/systems/performance). Ledger: [STATUS.md](STATUS.md). Gate B benches use `extern c` / `f64`. Prove the compiler with `scripts/selfhost-test` and `scripts/selfhost-verify`.

## Optimization layers

| Layer | What runs | When |
|-------|-----------|------|
| **MIR** | const fold, DCE, CFG simplify, small inline, Orbit (affine wrap fold + unroll) | `--release`, `--release-fast` |
| **LLVM/clang** | inlining, vectorization, LTO | link step |
| **Runtime** | work-stealing executor, channels | concurrency programs |

Disable MIR opts for debugging codegen: `buraaq build --release --no-mir-opt`.

## Build profiles

| Flag | clang | MIR | LTO |
|------|-------|-----|-----|
| (default) | `-O0` | off | no |
| `--release` | `-O2` | on | no |
| `--release-fast` | `-O3` | on | thin (`-flto=thin`) |
| `--size` | `-Os` | on | no |

Gate B used `--release` so it matches C++ `-O2`. Use `--release-fast` when you have a profile that cares about the last percent.

## Build time and binary size

Most of a small program's build is spent in clang, not in Buraaq. The driver
only compiles the C runtimes the module actually calls
([COMPILER_ARCHITECTURE.md](COMPILER_ARCHITECTURE.md#13-runtime-linking)), which
is where the numbers below come from. A `buraaq new --cli` hello-world,
`--release`, median of three, on Windows with LLVM 21:

| | Runtimes compiled | Build | Binary |
|---|---|---|---|
| Every runtime | 11 | 2.12s | 362,496 B |
| Only what is called | 2 | 0.43s | 238,592 B |

The saving scales with how much of the standard library you avoid, so it is
largest for exactly the programs where build latency is felt most: small CLIs
and the edit-run loop. A program that opens a window and serves HTTP pulls those
runtimes back in and lands near the top row.

The runtimes that *are* needed get compiled once and cached as object files,
keyed by source, headers, flags and clang version. The first build on a machine
pays for that; later ones link the objects. Same program, `--release`:

| | Build |
|---|---|
| Cold cache | 1.20s |
| Warm cache | 0.15s |

Before the cache, clang re-parsed and re-optimised the same runtime sources on
every link — about three quarters of the wall time of a small release build.

### Against rustc

The same loop-and-arithmetic program in Buraaq and in Rust, `--release` against
`rustc -O`, median of five, both printing `999000`:

| | Compile | Binary |
|---|---|---|
| rustc 1.96 | 0.112s | 125,440 B |
| Buraaq | 0.122s | 240,128 B |

Roughly parity, down from 4.2x slower. Buraaq's binaries are still about twice
the size. Three things closed the gap, in order of what they were worth:

| Change | Build |
|---|---|
| (start) | 0.46s |
| Cache compiled runtime objects | 0.156s |
| Memoise the clang lookup, and find it on `PATH` instead of running it | 0.138s |
| Link only the Windows import libraries the chosen runtimes need | 0.122s |

Locating clang cost more than compiling the program: it ran `clang --version` to
check each candidate, and nothing cached the answer. The Buraaq frontend — parse,
typecheck, MIR, LLVM IR — is 0.006s of the 0.122s. Essentially all of what is
left is clang.

This is not a like-for-like comparison of compiler work. rustc borrow-checks a
program Buraaq does not, and links a prebuilt libstd where Buraaq links C
objects. Treat it as edit-build loop latency, not a claim about either design.

### Indexing text

`byte(s, i)` used to call `strlen(s)` to bounds-check, so reading one character
cost the length of the string and walking a string cost its length squared. Any
program that scans text paid this; the bootstrap compiler, which reads its input
a character at a time, paid it worst.

The runtime keeps the lengths of the strings most recently measured
(`buraaq_length_of` in `stdlib/runtime/buraaq_rt.c`), so a read is constant. The
slot reused is the one holding the shortest string, because the cost of a miss is
the length of whatever was dropped — round-robin eventually evicts the string
being scanned and restores the square with a smaller constant.

Bootstrap compiler, emitting LLVM IR for a single function:

| statements | before | after |
|-----------:|-------:|------:|
| 400 | 0.18s | 0.026s |
| 1,600 | 3.85s | 0.064s |
| 6,400 | — | 0.212s |

Microseconds per byte now *fall* as the input grows (1.93 → 1.11 → 0.88), which
is fixed startup amortising over more work.

A cached length is only valid while the pointer still refers to the string it was
measured from, so every `free` and `realloc` in the runtime goes through
`bq_free` / `bq_realloc`, which forget them.
`runtime_releases_memory_through_the_cache_aware_free` fails if a release is ever
written that does not.

## Measuring MIR optimizations

Each compile records `OptStats`:

- `const_folds` — folded arithmetic/branches
- `dead_stmts_removed` — unused assignments eliminated
- `blocks_removed` — unreachable CFG blocks
- `calls_inlined` — trivial call sites expanded

Run product proof: `powershell -File scripts/selfhost-verify.ps1`

## Benchmark suite

```powershell
cd benchmarks
.\run_suite.ps1 -Release
.\run_suite.ps1 -ReleaseFast
```

Languages: `cpp/`, `rust/`, `go/`, `zig/`, `buraaq/`

Results: `benchmarks/results/suite/report-*.txt`

**Policy:** unfavorable Buraaq results are published. Investigate before claiming parity.

## Known gaps

- No monomorphization / devirtualization in MIR yet
- No escape analysis / stack promotion
- No bounds-check elimination pass
- Generic code lowers without specialization

## Tuning tips

1. Prefer `--release-fast` for production binaries on supported platforms
2. Keep hot loops free of unnecessary allocations — ownership moves have zero cost when optimized
3. Use `ref` borrows to avoid moves in inner loops
4. Write the obvious `while` or `for`. Orbit will close wrapping affine recurrences; forced unroll / alwaysinline made `integer_sum` worse when the algebra was missing
5. Profile with external tools (perf, VTune)
