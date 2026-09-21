# Compiler internals

High-level map of the product compiler (`compiler-buraaq/`).

## Pipeline

```
.bq source
  → Lexer (`src/lexer.bq`)
  → Parser (`src/parser.bq`)
  → Names (`src/names.bq`)
  → MIR subset (`src/mir.bq`)
  → LLVM IR text (`src/llvm.bq`)
  → clang link + `stdlib/runtime/buraaq_rt.c`
  → native executable
```

The driver is `src/main.bq`: `new` / `run` / `build` / `test` / `doctor` / `script` / `-e` / interactive shell.

## Modules

| File | Role |
|------|------|
| `src/scan.bq` | Byte scan helpers |
| `src/lexer.bq` | Tokens |
| `src/parser.bq` | Recursive-descent AST events |
| `src/names.bq` | Functions, params, locals, builtins |
| `src/mir.bq` | Mid-level ops for the guest subset |
| `src/llvm.bq` | LLVM text + signatures + typed print |
| `src/main.bq` | Product CLI |
| `boot/stage0.ll` | Clone seed (clang-link, then rebuild) |
| `golden/` | Frozen tokens / AST / sample |
| `selftest/main.bq` | `buraaq test` gold (`413489`) |

## Service runtime

`stdlib/runtime/buraaq_server.c` is the TLS + HTTP/1.1 + libpq loop behind `std.keel`. `stdlib/runtime/buraaq_lumen.c` is the native window behind `std.lumen`. Application code should not call `buraaq_http_listen` or Win32 directly. See [SERVICE.md](SERVICE.md), [LUMEN.md](LUMEN.md), and [STACK.md](STACK.md).

## Ship

`buraaq pack` / `ship` / `dock` use hashed `.bur` archives. See [SHIP.md](SHIP.md).

## Bootstrap

Clone + clang + `boot/stage0.ll` rebuilds the compiler. Track: [BOOTSTRAP.md](BOOTSTRAP.md).

## Tests

```text
powershell -File scripts/selfhost-test.ps1
powershell -File scripts/selfhost-verify.ps1
```

Invalid `.bq` must never abort the compiler process (except OOM).
