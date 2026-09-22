# Buraaq 1.0 stable API surface

This is the freeze list for language + stdlib entry points in **Buraaq 1.0**.
Engineering ledger: [STATUS.md](STATUS.md).

## Language

Frozen syntax: `fn`, `struct`, `enum`, `trait`, `impl`, `use`, `module`,
`if` / `elif` / `else`, `while`, `break`, `continue`, `return`, `spawn`,
`unsafe`, `drop { }`, `extern c`, `test` / `expect`.

Ownership: move by default, `ref` / `ref mut`, GFA (E0302 / E0312).

## CLI

| Command | Contract |
|---------|----------|
| `buraaq new NAME --cli` | Hello CLI scaffold |
| `buraaq run [FILE.bq]` | Build and execute |
| `buraaq build [FILE.bq [OUT]]` | Native executable via LLVM + clang |
| `buraaq test [FILE.bq]` | Run `selftest/main.bq` or a file |
| `buraaq doctor` | Clang, runtime, scripting line |
| `buraaq` / `repl` / `shell` | Interactive shell (clang per line) |
| `buraaq -e SNIPPET` | Compile and run a one-liner |
| `buraaq script FILE.bq` | Compile and run a `.bq` file |
| `buraaq --version` | `buraaq 1.0.0 (self-hosted)` |
| `buraaq -C DIR <cmd>` | Run the command with that project directory |

Install: `install.ps1` / `install.sh`.

## Standard library (stable names)

| Module | Functions |
|--------|-----------|
| `std.io` | `print`, `println` (text / int / float / bool), `eprintln`, `read_line` |
| `std.fs` | `read`, `write`, `exists` |
| `std.text` | `len`, `concat`, `eq`, `byte`, `slice` |
| `std.math` | `abs_int`, `sqrt`, `min`, `max`, `sin`/`cos`/`tan`, `exp`/`log`/`pow`, `pi`, `clamp`, `lerp` |
| `std.grid` | `zeros`, `ones`, `eye`, `row`, `at`, `put_at`, `dot`, `matmul`, `sum`, `mean` — numeric arrays |
| `std.hold` | `hold`, `stow`, `pick`, `keep`, `from_csv`, `col_mean` — named columns |
| `std.stream` | `stream`, `wire`, `say`, `hear`, `hangup`, `run_stream`, `clip`, `shot`, `heard` — live frames (text + audio/video) |
| `std.gfx` | `canvas`, `ink`, `wipe`, `plot`, `box`, `dash`, `flip`, `held`, `pulse`, `play`, `tone`, `hush` — game canvas + sound |
| `std.time` | `now_ms`, `now_sec`, `sleep`, `ms`, `sec` |
| `std.os` | `getenv`, `args`, `arg` |
| `std.keel` | `page`, `api`, `store`, `key`, `origin`, `call`, `run` — TLS APIs. `std.service` is the old name. [STACK.md](STACK.md) |
| `std.lumen` | `app`, `heading`, `note`, `field`, `button`, `bind`, `show` — native HD UI. [LUMEN.md](LUMEN.md) |
| `std.flowdesk` | `desk`, `show` — borderless Windows shell + Vein (UI Automation text) |
| `std.http` | `get` — `file://` all hosts; `https://` via WinINet (Windows) or OpenSSL when linked |
| `std.db` | `connect`, `connected`, `exec`, `quote`, `disconnect` |
| `std.json` | `parse`, `Value.field` (string or number/bool token) |
| `std.crypto` | `sha256` — real SHA-256 hex |
| lists / maps | `[1, 2]`, `["a"]`, `{ "k": v }` — guest literals, not `List[T]` / `Map[K,V]` constructors |

## C runtime

Symbols in `stdlib/runtime/buraaq_std.h` are the FFI boundary. Do not call
`buraaq_runtime_init` from user `main`; the C `main` trampoline owns process
startup. `buraaq_rt_set_args` is invoked by the generated `main`.
