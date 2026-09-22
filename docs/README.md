# Buraaq documentation

**Source of truth for what `dist/buraaq` compiles:** [SYNTAX_REFERENCE.md](SYNTAX_REFERENCE.md). Older spec drafts (`LANGUAGE_SPEC`, `ERROR_MODEL` target `Result`) say so at the top.

Read in this order. Skip what you do not need.

## Start

1. [Install](../README.md#install) — `install.ps1` / `install.sh`, then `buraaq doctor`
2. [Syntax](SYNTAX_REFERENCE.md)
3. [The Book](BURAAQ_BOOK.md)
3b. [Scripting](SCRIPTING.md) — `buraaq` / `-e` / `script` (clang per snippet)
4. [Standard library](STDLIB.md) — including **Stream**, **Gfx**, **Hold**, **Grid**
5. [The stack](STACK.md) — Keel, Stream, Gfx, Hold, Grid, Ship, Dock, Land, Mind, Strata
6. [Buraaq AI](AI.md) — `buraaq ai`, planner, OpenAI-compatible serve
6b. [Strata](STRATA.md) — GPU → RAM → NVMe model residency

## Ship an API

- [Keel](SERVICE.md) — TLS pages and REST
- [CRUD walkthrough](CRUD_API.md)
- [Ship](SHIP.md) — pack, dock, land (Hetzner, AWS, Azure, GCP, bare metal)

## Systems

- [Memory safety](MEMORY_SAFETY.md)
- [Ownership / memory model](MEMORY_MODEL.md)
- [Concurrency](CONCURRENCY.md)
- [Unsafe](UNSAFE_BURAAQ.md)
- [Performance](PERFORMANCE.md)
- [Gfx](GFX.md) — game canvas and sound (`canvas` / `flip` / `held`); **NOVA** in `stdlib/examples/nova.bq`
- [Lumen](LUMEN.md) — optional operator console (not the game surface)
- `std.flowdesk` — borderless Windows shell + Vein (see [STDLIB.md](STDLIB.md); demo in private `buraaq-play`)
- [Editors](../editors/README.md) — VS Code, Vim, Neovim, Sublime, Helix, Zed, JetBrains

## Language internals (contributors)

- [Status](STATUS.md) — 1.0 evidence and remaining hardening
- [Bootstrap](BOOTSTRAP.md) — product compiler is self-hosted; prove the chain
- [Compiler architecture](COMPILER_ARCHITECTURE.md)
- [Internals](INTERNALS.md)
- [Roadmap](ROADMAP.md)

## Policy

- [Stability](STABILITY.md)
- [Security](../SECURITY.md) — legitimate testing only; Buraaq cooperates with agencies
- [Porting](PORTING_FROM_C.md) from [C](PORTING_FROM_C.md), [C++](PORTING_FROM_CPP.md), [Rust](PORTING_FROM_RUST.md)

Public guides: [buraaq.dev/docs](https://buraaq.dev/docs)
