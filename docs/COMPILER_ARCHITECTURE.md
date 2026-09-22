# Buraaq Compiler Architecture

The Buraaq compiler (`buraaq`) is a multi-phase, incrementally compiled, LLVM-backed toolchain designed for fast rebuilds, excellent diagnostics, and native code quality competitive with Clang `-O2`.

**Guest today:** `compiler-buraaq` emits LLVM text and links with clang. HIR `throws` → `Result` below is the target pipeline. The emitter implements `raise` / `?` / `??` as empty values, `[…]` lists, and `{ "k": v }` maps.

---

## 1. Design Goals

| Goal | Architectural mechanism |
|------|-------------------------|
| Fast incremental builds | Fine-grained artifact cache keyed by `(module, phase, input_hash)` |
| Clear errors | Source-span tracking from lexer through MIR; structured suggestions |
| Safe semantics | Guarded Flow Analysis on MIR before codegen |
| Predictable output | Monomorphized generics; minimal runtime |
| Single entry point | Driver subcommands: `run`, `build`, `test`, `doctor`, `script`, `-e`, shell |

---

## 2. Toolchain Components

```
┌─────────────────────────────────────────────────────────────────┐
│                        buraaq (driver)                          │
├─────────┬─────────┬─────────┬─────────┬─────────┬───────────────┤
│  init   │  get    │  build  │  test   │  fmt    │  lsp-server   │
└────┬────┴────┬────┴────┬────┴────┬────┴────┬────┴───────┬───────┘
     │         │         │         │         │            │
     ▼         ▼         ▼         ▼         ▼            ▼
 buraaq.pkg  registry  compiler  testrt   formatter    buraaq-lsp
 resolver                       harness
```

### 2.1 Compiler modules (implementation)

| Module | Responsibility |
|--------|----------------|
| `src/lexer.bq` | UTF-8 lex + indent tokens |
| `src/parser.bq` | Recursive descent → AST events |
| `src/names.bq` | Module graph, imports, name binding |
| `src/mir.bq` | Mid-level IR subset |
| `src/llvm.bq` | MIR → LLVM IR text |
| `src/main.bq` | CLI: `new` / `run` / `build` / `test` / `doctor` / `script` / `-e` / shell |

Bootstrap: the product compiler is `compiler-buraaq/`, seeded by `boot/stage0.ll` + clang. Track: [BOOTSTRAP.md](BOOTSTRAP.md).

---

## 3. Compilation Pipeline

```
 .bq source
    │
    ▼
┌────────┐
│ Lexer  │  INDENT/DEDENT, comments stripped, spans recorded
└───┬────┘
    ▼
┌────────┐
│ Parser │  AST with error recovery for LSP
└───┬────┘
    ▼
┌──────────┐
│ Resolver │  Module paths, `use`, visibility, symbol tables
└───┬──────┘
    ▼
┌────────┐
│  HIR   │  Desugar: methods, `throws`, `?`, `async`, `for` loops
└───┬────┘
    ▼
┌─────────┐
│ Typeck  │  Hindley-Milner local inference + trait constraints
└───┬─────┘
    ▼
┌──────────────────┐
│ Monomorphization │  Generate specialized HIR/MIR per type instance
└───┬──────────────┘
    ▼
┌────────┐
│  MIR   │  CFG, explicit moves/borrows/drops, coroutine states
└───┬────┘
    ▼
┌────────┐
│  GFA   │  Memory + Send/Sync + async suspend checking
└───┬────┘
    ▼
┌─────────────┐
│ MIR opts    │  DCE, inline, const prop, bounds check elimination
└───┬─────────┘
    ▼
┌──────────────┐
│ LLVM codegen │  BIR lowering → LLVM IR
└───┬──────────┘
    ▼
 Object file (.o / .obj)
    ▼
 Linker (lld / link.exe / ld)
    ▼
 Native binary
```

---

## 4. Intermediate Representations

### 4.1 AST

Preserves syntactic structure for formatter and macro expansion (future).

### 4.2 HIR (High-level IR)

- Methods as free functions with `self` param: `Point.distance(p)` internally `distance(self: Point)`.
- `throws` functions return `Result[T,E]` in HIR.
- `async:` blocks as state-machine constructors.
- Pattern matches lowered to decision trees.

### 4.3 MIR (Mid-level IR)

Three-address code with basic blocks:

```
bb0:
  %1 = alloc List[i32]
  %2 = call List.new()
  move %dest <- %2
  drop %1
  return
```

MIR statements: `Assign`, `Move`, `Borrow(Shared|Mut)`, `Drop`, `Call`, `Await`, `Return`.

All safety-critical operations explicit for GFA.

### 4.4 LLVM IR

Standard LLVM 18 dialect. Buraaq uses Itanium name mangling for link compatibility with C++ where needed.

---

## 5. Incremental Compilation

### 5.1 Cache layout

```
.buraaq-cache/
  packages/<pkg_id>/
    modules/<module_path>.<phase>.bin
    mir/<symbol_hash>.ll
    deps.graph
```

### 5.2 Invalidation rules

| Change | Recompile |
|--------|-----------|
| Source text hash | That module, all importers transitively |
| Public API signature | Dependents only (API hash) |
| `buraaq.pkg` deps | Lockfile resolution + affected packages |
| Target triple / opt level | Full codegen relink |

### 5.3 Parallelism

- Module-level parallelism in typeck/MIR/GFA (DAG order).
- LLVM codegen parallel per function.
- Default thread pool = `num_cpus`.

---

## 6. Diagnostic Architecture

### 6.1 Diagnostic structure

```json
{
  "level": "error",
  "code": "E0301",
  "message": "cannot borrow `list` as mutable while shared borrow is active",
  "span": { "file": "main.bq", "line": 12, "col": 5 },
  "labels": [
    { "span": {...}, "message": "shared borrow starts here" },
    { "span": {...}, "message": "mutable borrow attempted here" }
  ],
  "help": "split borrows with a nested block or clone the data",
  "fixes": [{ "kind": "add_block", "patch": "..." }]
}
```

### 6.2 Error codes

Stable `E` codes documented in `docs/errors/`. Categories:

- `E01xx` parse/resolve
- `E02xx` type
- `E03xx` borrow/ownership (GFA)
- `E04xx` concurrency (Send/Sync)
- `E05xx` async
- `E06xx` FFI/unsafe

### 6.3 `buraaq explain E0301`

Prints extended rationale with timeline diagram (borrow checker visualization).

---

## 7. Optimization Pipeline

### 7.1 MIR passes (target-independent)

| Pass | Purpose |
|------|---------|
| `SimplifyCfg` | Remove empty blocks |
| `Inline` | Heuristic inlining (`#[inline]` hint) |
| `ConstProp` | Fold constant expressions |
| `DeadStoreElim` | Remove unused assignments |
| `CheckElim` | Remove proven-safe bounds/null checks |
| `DropElision` | Remove no-op drops |

### 7.2 LLVM passes

Profile-driven `-O2` default. ThinLTO optional (`--release-fast`).

---

## 8. Debug Information

- DWARF 5 on macOS/Linux
- PDB on Windows via LLVM
- Source maps: Buraaq spans embedded in `!DIFile`
- `-g` includes full variable locations; `-gline-tables-only` for release

---

## 9. Sanitizers and Verification

Debug profiles support:

- AddressSanitizer (`--sanitize address`)
- ThreadSanitizer (`--sanitize thread`)
- MemorySanitizer (tier-2 platforms)

 MIR-level `--verify` runs GFA twice (redundant check) in CI.

---

## 10. Language Server (LSP)

`buraaq lsp-server` shares frontend with compiler:

| Feature | Phase reused |
|---------|--------------|
| Diagnostics | parse → typeck → GFA (truncated) |
| Go to definition | resolver |
| Hover types | typeck inference result |
| Rename | resolver + patch applicator |
| Format | `buraaq_fmt` AST round-trip |
| Code actions | diagnostic fixes |

Incremental: LSP keeps per-file HIR cache; recomputes on edit with debounce.

---

## 11. Testing Infrastructure

| Layer | Tool |
|-------|------|
| Lexer/parser | Snapshot tests |
| Typeck/GFA | `// expect-error E0301` directives |
| Codegen | LLVM FileCheck comparisons |
| Integration | `buraaq test` runs `*_test.bq` |
| Performance | Benchmark suite vs C/Rust equivalents |
| UI tests | Diagnostic snapshot `.stderr` files |

---

## 12. Cross-Compilation

```
buraaq build --target aarch64-unknown-linux-gnu
```

Driver downloads standard library built for target + LLVM target support check. Linker: `lld` cross-link bundled.

---

## 13. Runtime Linking

| Profile | Linked runtime |
|---------|----------------|
| Default | `libburaaq_rt.a` — panic, thread spawn hooks, async I/O (if used) |
| `#![no_std]` | No default runtime; user provides panic handler |
| `#![no_async]` | Excludes async scheduler |

Dead code elimination strips unused runtime modules.

### Which C runtimes get compiled

`buraaq_rt.c` (printing, allocation, conversions) and `buraaq_std.c` (text, file,
math, os, time, process) are the base and always link. The rest are feature
runtimes: UI (`buraaq_lumen.c`), server and Postgres (`buraaq_server.c`),
threads and channels (`buraaq_runtime.c`), matrices (`buraaq_grid.c`),
dataframes (`buraaq_hold.c`), sockets (`buraaq_stream.c`), boards
(`buraaq_board.c`), flowdesk, and the AI client.

`runtime_paths_for` in `driver/src/compile.rs` picks a feature runtime only when
the emitted module references one of the symbols it owns. Every module
*declares* all runtime symbols, so `declare` lines are skipped and only call
sites count. Each entry is keyed by the symbol prefixes it exclusively owns —
`buraaq_std.c` owns `buraaq_http_get_body` while `buraaq_server.c` owns the other
`buraaq_http_*` symbols, so the server entry lists its HTTP symbols one by one
rather than matching `buraaq_http_`.

The feature runtimes have no dependencies on each other, which is what makes
dropping one safe. Adding a cross-reference between two of them would break that
and the map would need to grow edges.

Before this, every binary compiled all eleven files. A `println` program spent
about 1.7s of its build compiling a UI toolkit, an HTTP server and a thread pool
it never called, and carried them in the output.

---

## 14. Security Considerations

- `unsafe` audit trail: optional `buraaq build --deny unsafe` for crates
- Supply chain: lockfile hashes; `buraaq get --verify` checks signatures when registry live
- Deterministic builds: `SOURCE_DATE_EPOCH` support for reproducible objects

---

## 15. Repository Layout (Compiler)

```
compiler-buraaq/
  src/             # lexer, parser, names, mir, llvm, main
  boot/stage0.ll   # clone seed
  golden/
  selftest/
stdlib/            # buraaq-std sources + C runtime
scripts/           # selfhost-test, selfhost-verify, pack-dist
```

This architecture supports the roadmap milestones from bootstrap through v1.0 GA without redesigning core phases.
