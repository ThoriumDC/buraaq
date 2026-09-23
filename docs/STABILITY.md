# Buraaq 1.0 stability policy

## Versioning

Buraaq follows **Semantic Versioning 2.0.0**:

| Version | Meaning |
|---------|---------|
| `0.x.y` | Pre-1.0 — language and tooling may evolve; breaking changes documented in CHANGELOG |
| `1.0.0` | First stable language + ABI snapshot |
| `1.x.y` | Compatible within major; patch = bugfix only |

**1.0 (public):** the day-to-day native subset — syntax below, Keel/Ship/Dock/Land, Stream/Gfx/Hold/Grid, and Gate B loops — is the language you install from [buraaq.dev](https://buraaq.dev). Remaining hardening (7-day fuzz clock) does not un-ship that surface. The product compiler is self-hosted (M26). Optional `buraaq` / `-e` / `script` compile snippets with clang. See [STATUS.md](STATUS.md).

## Buraaq 1.0 syntax guarantee

At 1.0, the following are **frozen**:

- Brace blocks `{ }` (4 spaces official inside them)
- Core keywords: `fn`, `mut`, `struct`, `enum`, `trait`, `impl`, `match`, `async`, `spawn`, `raise`, …
- `print` / `println`, `"Hello, {name}"`, `[1, 2]`, `["a"]`, `{ "k": v }`
- Ownership + borrow rules as documented in `MEMORY_MODEL.md` (`give` reserved; guest bindings are `x =`)
- Module system: `module`, `use io` / `use std.io`, unique `std.*` auto-import, `buraaq.pkg`
- Error model: `raise` / `?` / `??` (empty values). Tagged `Result` is the target, not the guest freeze.

**May still grow after 1.0:** generic constraints beyond the int/text/float/struct copies, hosted packages.buraaq.dev. Diagnostic **IDs** stay stable; wording may improve.

## ABI policy (1.0 target)

- C FFI is the **stable foreign boundary** (`extern c` blocks)
- Buraaq-to-Buraaq ABI across compiler versions is **best-effort** within 1.x
- 1.0 will publish a platform calling convention document (SysV x64, Windows x64)

## Compatibility policy

- **Source:** `buraaq check` on 1.0 code must pass on 1.x patch releases
- **Packages:** `buraaq.pkg` semver ranges; lockfile pins exact versions
- **Tooling:** LSP schema versioned; editors pin minimum `buraaq` CLI version

## Package policy

- Registry-agnostic: projects are directories + `buraaq.pkg` + `buraaq.lock`
- In-tree index is `packages/index.json`; hosted `packages.buraaq.dev` is next
- No implicit network fetch in 1.0 core (explicit `buraaq add` only)

## Unsafe code policy

- `unsafe` blocks required for: raw pointer dereference, unchecked FFI, inline asm (future)
- Unsafe **cannot** bypass borrow checker at compile time — only documented escape hatches
- Stdlib wraps OS/FFI behind safe APIs where possible; unsafe concentrated in `std.os`, `std.crypto`

## Deprecation

- Features marked `@deprecated` in docs + compiler warning for one minor release before removal
