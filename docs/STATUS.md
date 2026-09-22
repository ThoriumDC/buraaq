# Buraaq 1.0 status

The **language product** is 1.0: you install Buraaq, write `.bq`, and ship native binaries. This page is the measured ledger — not a second product.

```text
Language 1.0: public
Compiler tag 1.0.0: shipped (Gate D 7-day fuzz deferred)
```

Do not print `BURAAQ 1.0 RELEASE GATES: PASS` until A–J all pass, including a real 7-day fuzz clock. Public 1.0 is the language product, not that banner.

## What shipped

- Native multi-module programs, typed `print`/`println`, project auto-import of unique `std.*`
- Growable lists `[1, 2]` / `["a"]`, maps `{ "k": v }`, `raise` / `?` / `??`
- **Stream** (`std.stream` text + `clip`/`shot`), **Gfx** (`std.gfx` canvas/sound; play `stdlib/examples/nova.bq`), **Hold** (`std.hold`), **Grid** (`std.grid`), expanded `std.math`
- Keel TLS APIs + Postgres (including Neon `sslmode=require`)
- Ship `.bur` / Dock `:7422` / Land kits (`aws`, `azure`, `gcp`, `hetzner`, `bare`)
- Compiler frontend in Buraaq: lexer, parser, names, MIR, LLVM text (M3–M10)
- Install copies packaged `dist/buraaq` + clang sidecar
- Self-hosted CLI: shell / `-e` / `script` / `run` / `build` / `test` / `doctor`
- Forge (private `buraaq-play/forge`): ownership, spawn, generics, Keel ledger, pack, Hetzner land — not in this public tree
- Compiler stress/fuzz smoke: 600 mutated programs, 400 random-byte, 10 clang compile+run

## Gates

| Gate | Status | Evidence |
|------|--------|----------|
| A Multi-module native | **PASS** | 10-module exe prints `42`; Forge is 4 modules |
| B Perf vs C++ `-O2` | **PASS** | Orbit fold closes `integer_sum` (**0.00×**, same n); worst `fib_iter` **1.02×** |
| C–C‴ Bootstrap M3–M25 | **PASS** | CI `selfhost`; `boot/stage0.ll` + clang; mut/float/typed print |
| D 7-day fuzz | **deferred** | Public 1.0 shipped; wall-clock fuzz continues after launch |
| E Safety | **PASS** | GFA + typed drop + loop `defer` on break/continue |
| F Wrong-code | **PASS** | UI corpus + inverted spans no longer panic |
| G Stdlib 1.0 APIs | **PASS** | fs/math/sha256/json numbers/HTTPS GET; Keel+Neon |
| H Install | **PASS** | `install.ps1` / `install.sh` + Land kit |
| I Docs | **PASS** | This tree + [buraaq.dev](https://buraaq.dev) |
| J Tier-1 CI | **PASS** | Only `selfhost`: clang + stage0 + verify |

## Benchmarks (Gate B)

Equivalent-n, Buraaq `--release` vs C++ `-O2`, clang 22. n was not reduced.

| Bench | n | C++ `-O2` | Buraaq | Ratio |
|-------|---:|----------:|-------:|------:|
| nested_loop | 1e4² | 0.010s | 0.002s | **0.16×** |
| integer_sum | 1e8 | 0.015s | <0.001s | **0.00×** |
| float_saxpy | 1e7 | 0.004s | 0.004s | **0.97×** |
| numerical_loop | 1e8 | 0.083s | 0.084s | 1.01× |
| fib_iter | 1e8 | 0.020s | 0.021s | 1.02× |

Orbit rewrites wrapping `sum = sum * 3 + i` to affine matrix doubling (O(log n) in wrapping i32). Same n=1e8; n=10 checksum is `44281`. C++ still runs the counted loop. n was not reduced.

## Self-host (honest)

The **compiler** is written in Buraaq. A clone with clang links `compiler-buraaq/boot/stage0.ll` and rebuilds, packs, tests, and CI-proves it. The C runtime lives in `stdlib/runtime/`. Proof: [BOOTSTRAP.md](BOOTSTRAP.md#prove-the-chain).

## Still hardening

- Gate D 7-day fuzz elapsed time (deferred from the public 1.0.0 tag)
- Package registry, DAP pretty-printers, channels
- POSIX HTTPS needs OpenSSL at link; JSON is field extract, not a full DOM
- Guest lowering of generics, trait-method dispatch, and real concurrent spawn
- Guest float arithmetic beyond literals (literals and typed print are M25)
- Language-tour compile is 60/60; mutex/channel/await/raw-pointer still use sequential/stub lowering (spawn is inlined)
- Interactive `buraaq` / `-e` / `script` compile a temp `.bq` with clang (`read("-")` is one stdin line)

## Known grain

- `{` inside `"..."` interpolates — keep JSON in files, not in Buraaq string literals
- `ok` is a keyword
- Pack the `.bur` on the **same OS** as the host (Windows ship will not run on Linux)
- On Windows, `spawn` then Keel/`libpq` SSL in the same process can AV; Forge runs spawn only under `BURAAQ_COVERAGE_ONLY=1`

## Hetzner Land

`buraaq land --cloud hetzner` writes `target/land/`. Forge was landed as a native process on a Hetzner VM (HTTP 8080 / TLS 8443) without touching existing Docker services on 80/443/3000/3100. Postgres credentials stay in host env, never in git.
