# The Buraaq Book

*A narrative guide for systems programmers who want less ceremony and more clarity.*

## Part I — Why Buraaq?

Modern systems languages split into two failures:

1. **C/C++** — full control, minimal safety, maximal cognitive load
2. **Rust** — safety, but the compiler feels like a second job

Buraaq asks: *what if memory safety and performance were defaults, and the compiler taught instead of scolded?*

## Part II — First program

```buraaq
fn main() {
    println("Hello, Buraaq!")
}
```

`println` is unique in std, so there is no `use`. It takes text, int, float, or bool.

Run: `buraaq run`. No build system debates — `buraaq.pkg` + `buraaq` CLI.

## Part III — Ownership without jargon

Bindings are ordinary names. The guest does **not** lower `give x = …` (`give` is read as a variable). Write:

```buraaq
socket = connect("127.0.0.1", 8080)
send_all(socket, payload)
```

Need a pointer the callee must not free? **Borrow:**

```buraaq
send_all(ref socket, payload)
socket.close()
```

`give` remains a reserved word for a later clarity pass. Do not start a binding with it.

## Part IV — Errors that teach

Diagnostics include:

- Primary span (where you went wrong)
- Secondary spans (where the conflict started)
- Plain-language help
- Suggested fix (LSP code action)

See `docs/errors/` for stable error catalog entries.

## Part V — Concurrency one way

Threads, async tasks, channels, and mutexes share one runtime (`stdlib/runtime/`). No three async ecosystems.

```buraaq
ch = channel[i32](64)
spawn {
    ch.send(42)
}
v = ch.recv()
```

## Part VI — Tooling as a feature

The compiler is written in Buraaq. `dist/buraaq` is that compiler.

| Tool | Command |
|------|---------|
| Shell | `buraaq` / `buraaq repl` |
| One-liner | `buraaq -e "println(40+2)"` |
| File | `buraaq script FILE.bq` / `buraaq run FILE.bq` |
| Build | `buraaq build` |
| Test | `buraaq test` |
| Doctor | `buraaq doctor` |

The shell wraps each line in `fn main` and compiles it with clang (same language as AOT, not a second interpreter). See [SCRIPTING.md](SCRIPTING.md). Editor support: [editors/](../editors/README.md).

## Part VII — Performance

Buraaq lowers to LLVM. Use `--release-fast` for production. Measure with `benchmarks/run_suite.ps1`.

See [PERFORMANCE.md](PERFORMANCE.md).

## Part VIII — Production services

Complexity belongs in the runtime. You declare pages and APIs; `run()` binds TLS and serves.

```buraaq
fn main() {
    page("/", "public/index.html")
    api("items", "title, body")
    run()
}
```

See [STACK.md](STACK.md) for Keel → Ship → Dock → Land (AWS, Azure, GCP, Hetzner, bare metal).

Live frames are **Stream** (`say` / `hear`, plus `clip` / `shot` for files). Pixels and keys are **Gfx** (`canvas` / `flip` / `held`) — not Lumen. Run `stdlib/examples/nova.bq`. [GFX.md](GFX.md).

## Part IX — Stability and 1.0

Pre-1.0: expect evolution. At 1.0: syntax + ownership rules frozen per [STABILITY.md](STABILITY.md).

## Part X — Porting

- [From C](PORTING_FROM_C.md)
- [From C++](PORTING_FROM_CPP.md)
- [From Rust](PORTING_FROM_RUST.md)

## Part XI — What Buraaq is not

- Not a scripting language with hidden GC (optional `buraaq` / `-e` / `script` still compile native snippets)
- Not a Rust clone with different spelling
- Not "simple" by removing `unsafe` when you need FFI

**Simplicity through design, not deletion.**
