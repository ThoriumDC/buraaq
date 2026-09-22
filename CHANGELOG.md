# Changelog

All notable changes to Buraaq are documented here.

Format based on [Keep a Changelog](https://keepachangelog.com/).

## [1.0.0] — 2026-09-16

Public language 1.0. Site: [buraaq.dev](https://buraaq.dev). Engineering ledger: [docs/STATUS.md](docs/STATUS.md).

### Added

- Self-hosted compiler frontend in Buraaq: lexer, parser, names, MIR, LLVM text (M3–M10). Guest LLVM compiles `lexer.bq` / `parser.bq`.
- Install prefers `dist/buraaq` + LLVM sidecar (`buraaq doctor`). Cargo is optional for *using* the language.
- `std.stream` — live frames (WebSocket job): `stream`, `wire`, `say`, `hear`, plus `clip` / `shot` / `heard` for audio and video files
- `std.gfx` — game canvas and sound: `canvas`, `ink`, `wipe`, `plot`, `box`, `dash`, `flip`, `held`, `pulse`, `play`, `tone`, `hush`
- Games: `stdlib/examples/nova.bq` (neon arena shooter), `gfx.bq`, `snake.bq`; Stream A/V smoke `stream_av.bq`
- Guest `build_auto` links `buraaq_stream.c` and `buraaq_gfx.c` (`-lgdi32 -luser32 -lwinmm` on Windows)
- `std.hold` — named columns (Pandas job): `hold`, `stow`, `pick`, `keep`
- `std.grid` — numeric arrays (NumPy job): `zeros`, `dot`, `matmul`
- Expanded `std.math` scalars
- Typed `print` / `println` (text, int, float, bool) and project auto-import of unique `std.*`
- Text lists: `["a", "b"]`, `xs[i]`, `xs.push`, `for s in xs`
- `raise` returns empty (null / 0) after stderr; `?` forwards empty; `??` still defaults
- Map literals: `{ "Asim": 30 }`, `m["Asim"]`
- Keel API keys, CORS allow-lists, auto TLS certs, `BURAAQ_HTTP_PORT` / `BURAAQ_TLS_PORT`
- `buraaq ship HOST --bundle`, `buraaq build --emit-ir --target linux`
- Land kit for Hetzner / AWS / Azure / GCP / bare metal
- Forge example: ownership, concurrency, Keel + Postgres, pack, land
- Compiler stress/fuzz smoke (mutated programs + random bytes)
- Loop `defer` on `break` / `continue`

### Performance

Gate B vs C++ `-O2` (clang 22, equivalent n): integer_sum **0.00×** (Orbit wrapping affine fold), nested_loop **0.16×**, float_saxpy 0.97×, numerical_loop 1.01×, fib_iter **1.02×**. n was not reduced. Worst case published.

### Self-host

M11: guest LLVM compiles `llvm.bq`, links, and `dump_llvm` emits a module. rustc still builds the host CLI.

### Security

Legitimate-use policy; Buraaq administration cooperates with lawful agency requests. See [SECURITY.md](SECURITY.md).

## [Unreleased]

### Added

- Complete self-host chain (M21–M24): product CLI is the guest; `boot/stage0.ll` + clang clones without rustc; CI `selfhost` has no rust-toolchain
- M25: guest skips `mut`, lowers float literals, and types `print` / `println` / `print_int` / `print_float` / `print_bool` (including `print_stress.bq`)
- M26: `buraaq_rt.c` lives in `stdlib/runtime/`; pack/install/CI need only clang and the guest
- `scripts/selfhost-test` / `selfhost-verify` / `pack-dist` seed from a guest binary or `boot/stage0.ll`, never from a rustc-built host
- Guest lowering: `const` / `static` / `continue` / `defer`; `spawn` inlined; `trait` skipped; `impl` unwrapped; `async`/`await` stripped
- MIR optimization pipeline: constant folding, DCE, CFG simplification, small-call inlining
- `--release-fast` (`-O3` + thin LTO) and `--size` (`-Os`)
- Cross-language benchmark suite
- Buraaq Ship / Keel / Dock / Land
- Fuzz tests: lexer, package manifest parser
- `buraaq` / `repl` / `-e` / `script` compile snippets with clang (stdin line reader in `buraaq_rt.c`)
- Unique stdlib names auto-import; `use io` means `std.io`; `connect` / `show` / `keep` need `use db` or `use net` (and the matching pair)

### Changed

- Parser missing-expression diagnostic uses **E0102** (E0101 reserved for unknown names)
- Ownership/borrow errors use multi-span teacher-style diagnostics
- Stdlib C runtime tests link `buraaq_rt.c` and do not require `OUT_DIR`

### Removed

- Leftover rustc host (`compiler/`) and the rust-only `stdlib` crate (`Cargo.toml`, parse tests, alloc bench). The product path is `compiler-buraaq/` + clang.

### Fixed (self-host hardening)

- `xs[1].to_text()` was skipped as a generic type argument, so array index tests printed a stale int; only `Type[T].new` / `.bounded` / `{` skip the brackets
- `extern c` `...` varargs never advanced the parser, so `printf(..., ...)` grew IR until the machine ran out of RAM
- `buraaq -C DIR run` now sets the process directory to `DIR`, so project files like `data.txt` resolve
- Guest signatures treat `f64`/`f32` as float and `void` as void, so Gate B timers are not i32 subtracts

- `(a + b)` and any parenthesized expression read the operand that started it instead of the computed value, so struct field sums and guard conditions were wrong
- An unterminated `{` inside a string literal looped the emitter forever; a three byte file (`" {`) was enough. 17 of 200 random-byte inputs used to hang, now 0 of 300
- A tail `if` in a function body now returns its arm's value, and only when that arm ends in a real value of the function's own type
- Guest LLVM now lowers `Mutex.new` / `.lock()` / `guard[]`, `Channel[T].bounded` / `.send` / `.recv`, `spawn_task`/`await`, and `*mut` / `&mut x as *mut T` / `.is_null()` so the language tour compiles 60/60

### Fixed

- E0101 code collision between parser and resolver
- Inverted `Span` ranges no longer panic on fuzz input

## [0.1.0] — pre-1.0 development

Initial public development snapshot: lexer, parser, semantic analysis, MIR, LLVM codegen, stdlib, package manager, concurrency runtime, LSP.
