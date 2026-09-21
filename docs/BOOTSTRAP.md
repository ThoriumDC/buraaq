# Buraaq bootstrap track (M3–M26)

`compiler-buraaq/` is the **product compiler**, written in Buraaq. A milestone is **PASS** only when a test in this repo fails if the guest drifts.

The product compiler is Buraaq. A clone with clang links `compiler-buraaq/boot/stage0.ll`, rebuilds the guest, and runs `buraaq test`.

## Prove the chain

```text
powershell -File scripts/selfhost-test.ps1
powershell -File scripts/selfhost-verify.ps1
powershell -File scripts/pack-dist.ps1
# POSIX: bash scripts/selfhost-test.sh && bash scripts/selfhost-verify.sh && bash scripts/pack-dist.sh
```

| Step | Command | Passes when |
|------|---------|-------------|
| Seed | clang links `boot/stage0.ll` | binary `--version` contains `self-hosted` |
| Rebuild | guest `llvm` of `src/main.bq`, clang link | stage1 `--version` contains `self-hosted` |
| Selftest | `buraaq test compiler-buraaq/selftest/main.bq` | stdout `413489` |
| Golden | `buraaq run compiler-buraaq/golden/sample.bq` | stdout `5` |
| Product | `buraaq new hello --cli` then `buraaq run` | hello prints |
| Pack | `scripts/pack-dist` | `dist/buraaq` is the guest; no cargo |

CI is only `selfhost`: clang + `boot/stage0.ll`, no rust-toolchain.

This is dogfooding: each milestone is a Buraaq program the guest must compile, and a frozen golden or selftest fails if it drifts.

| Milestone | Guest in `compiler-buraaq/` | Evidence | Rust still required? |
|-----------|-----------------------------|----------|----------------------|
| **M3** Lexer | `src/lexer.bq` | `bootstrap_m3` — token kinds match `buraaq_lexer` on `golden/sample.bq` | Yes (host compiles the lexer) |
| **M4** Parser | `src/parser.bq` | `bootstrap_m4` — AST event stream matches Rust `Parser` on the same golden | Yes |
| **M5** Names | `src/names.bq` | `bootstrap_m5_names_match_rust` — fn/param/local/builtin/fnref match the host AST walk | Yes |
| **M6** MIR subset | `src/mir.bq` | `bootstrap_m6_mir_subset_matches_rust` — store/loop/br/icmp/const/call/ret and every operator match the host walk | Yes |
| **M7** LLVM text | `src/llvm.bq` | `bootstrap_m7_llvm_links_and_prints_five` — guest `.ll` clang-links; stdout is `5` | Yes (clang, not rustc) |
| **M8** Driver | `llvm build` in `src/main.bq` | `bootstrap_m8_guest_build_prints_five` — guest writes `.ll` and spawns clang through `std.process`; stdout is `5` | rustc not used for that file |
| **M9** Self-host (lexer/parser) | guest LLVM compiles `lexer.bq` / `parser.bq` | `bootstrap_m9_*` | rustc still builds `compiler-buraaq` (including `llvm.bq`) |
| **M10** Install | `dist/` + LLVM sidecar | `bootstrap_m10_clang_sidecar_lookup` | No (to *use* Buraaq) |
| **M11** Guest compiles `llvm.bq` | unbounded ABI tape, declare-on-bind, string weave, IR newlines | `bootstrap_m11_guest_llvm_compiles_llvm_bq` | Yes (host still rustc-built) |
| **M12** Fixpoint | guest rebuilds `llvm.bq`, then that build rebuilds it again | `bootstrap_m12_guest_rebuilds_llvm_without_rustc` — stage1 and stage2 emit byte-identical IR | No, for that rebuild |
| **M13** Modules | guest follows `use` across files | `bootstrap_m13_guest_resolves_modules` — three files, none reachable without resolving an import | No, for that build |
| **M15** Enums, `match` | tag constants and arm dispatch | `bootstrap_m15_guest_compiles_enums_and_match` | No, for that build |
| **M16** Loops, subscripts | `for v in a..b`, `xs[i]`, `xs[i] = v` | `bootstrap_m16_guest_compiles_loops_and_indexing` | No, for that build |
| **M17** Whole guest | guest compiles every module, and that build reproduces itself | `bootstrap_m17_guest_rebuilds_the_whole_guest` — all five stages match the host-built guest; stage1 and stage2 emit byte-identical IR | No, for that rebuild |
| **M18** Full-surface parity | goldens cover every token kind and every construct, not one small program | `bootstrap_m3`/`m4`/`m5`/`m6` on `golden/tokens.bq` and `golden/ast.bq`, `bootstrap_m9_*` for the guest-*built* lexer and parser, `bootstrap_guest_ir_agrees_on_return_types` | No, for that rebuild |
| **M19** Types from signatures | a call's return type is the callee's `->`, per-file diagnostics | `bootstrap_m19_signatures_type_calls`, `bootstrap_m19_diagnostics_name_the_source_file` | No, for that rebuild |
| **M20** rustc-off of the guest | guest rebuilds `compiler-buraaq` with clang, rustc not on PATH | `bootstrap_m20_guest_rebuilds_without_rustc` — stage1 and stage2 still byte-identical | No, rustc is not used for that rebuild |
| **M21** Product compiler | guest `run` / `build` / `doctor`; pack-dist never calls cargo | `bootstrap_m21_guest_is_the_product_compiler`, `bootstrap_m21_pack_dist_does_not_invoke_cargo` | No. rustc is not used to pack or to run the compiler |
| **M22** Product path | `new --cli`, project `run`/`build` via `-C`; `module` / `loop` / `unsafe` | `bootstrap_m22_new_and_project_run`, `bootstrap_m22_module_loop_unsafe` | No |
| **M23** rustc-free proof | `const` / `continue` / `defer`; skip `type` / `import` / `where`; `buraaq test` | `bootstrap_m23_const_continue_defer_and_test`, `scripts/selfhost-test` | No. rustc is not used to test or rebuild the compiler |
| **M24** rustc-free clone | `spawn` inlined; `trait`/`impl`/`async`/`await` skipped or unwrapped; committed `boot/stage0.ll` | `bootstrap_m24_spawn_trait_impl_async`; CI `selfhost` has no rust-toolchain | No. A clone with clang and `stage0.ll` proves itself |
| **M25** mut, float, typed print | skip `mut`; float literals; `print`/`println`/`print_int`/`print_float`/`print_bool` | `bootstrap_m25_mut_float_typed_print` — `print_stress.bq` gold | No |
| **M26** rustc tree not required | runtime in `stdlib/runtime/buraaq_rt.c`; CI is `selfhost` only | `find_rt` / pack / install / CI have no rustc | No |

## What M3–M10 already proved

The lexer is not a sketch. `bootstrap_m3` compiles `compiler-buraaq` with the host, runs it on `golden/sample.bq`, and asserts stdout kinds equal Rust `TokenKind::golden_name()`.

M4–M6 dump events from the same goldens. M7/M8 lower `print(add(2,3))` to LLVM, link with `buraaq_rt.c`, and run: stdout is `5`. M8 is the Buraaq driver invoking clang; the host is only used to *build* that driver.

**M9** is the guest LLVM emitter compiling `lexer.bq` and `parser.bq` (plus a thin `main`). Those guest-built programs still pass the M3 kinds golden and a non-empty M4-style dump.

**M11** is that same guest compiling `llvm.bq` itself: dump_llvm prints a module with `define`. At that milestone rustc still built the host CLI. M21–M26 removed that host.

**M12** closes the loop. stage1 is `llvm.bq` compiled by the rustc-built guest; stage2 is `llvm.bq` compiled by stage1. Both are then run on the compiler's own source, and the two outputs must be byte-identical. Equality is the point: it says the emitter no longer depends on which compiler built it, so the rustc-built guest can be thrown away and the result does not change. `scripts/bootstrap-fixpoint.ps1` runs the same three stages by hand and prints the first differing line when they disagree.

**M13** removes the last piece of scaffolding around the fixpoint. The guest read
one file, so `scan.bq` and `llvm.bq` had to be concatenated with an entry point
bolted on before it could compile itself — by a PowerShell script in one place
and by the Rust tests in another, each with its own copy of the weave. It now
resolves `use m.x` itself: `m.bq` is read from beside the entry file or from its
`src/`, transitively, each module once. `compiler-buraaq/boot_llvm.bq` is the
entry point, and it reaches `llvm.bq` and `scan.bq` the same way any program
would. Modules are concatenated in discovery order; the emitter resolves names
out of the text it is given, so order does not matter, and a module that cannot
be found is reported rather than skipped.

Diagnostics from the guest carry line numbers within the combined unit, not
within the file the line came from. That is worth fixing before the frontend
ports land, since by then most errors will be about somebody else's file.

## How the guest types a local

Every local gets two slots, `%name.i` for an int and `%name.p` for text, and each
use has to pick one. That choice used to come from a hardcoded list of names
inside `llvm.bq`: `line` meant text, `w` meant an int, in every function. Code
that disagreed loaded from the slot it never stored to, which is how two of the
bootstrap miscompiles happened, and adding a variable meant checking the list.

`l_text_locals` now walks a function body before emitting it and classifies each
local by what is assigned to it, seeded with the text parameters. It runs three
passes so `b = a` picks up an `a = concat(...)` that appears later. There is no
list of names left.

`mut` on a local or parameter is skipped. `mut n = 0` stores to `%n.i`.

## What the guest can actually compile

`int`, `text`, `bool`, `float` literals; `mut`; `if`/`elif`/`else`; `while`; `loop`; `for v in a..b`; `unsafe`
as a scope; `module` lines; `const` / `static` integers; `continue`; `defer`;
`spawn` inlined; `trait` skipped; `impl` methods as ordinary functions;
`async`/`await` stripped; `type` / `import` / `where` skipped; functions and calls;
string interpolation; structs; enums and `match`. That is the whole list.

An enum variant is a tag and nothing else: `Color.Red` is the constant 0, and a
variant carries no payload. Payloads want a heap block with the tag in the first
slot, which the struct lowering could already do, but they are only worth having
with generics — an `Option` that cannot say what it holds is not much of one.

`match` evaluates its subject once and compares it against each arm in turn.
Patterns are an integer, an `Enum.Variant`, or `_`. Nothing checks
exhaustiveness, and a subject matching no arm falls out of the match having run
nothing. Arms are a chain of branches rather than a jump table; tags are small
and contiguous so a table would be faster, but it needs the arms sorted and the
gaps filled, and the emitter has nowhere to hold that while streaming a block.

`for v in a..b` counts, with `..=` for an inclusive end. The counter starts one
below `a` and steps at the top of the loop, so the back edge and the exit are the
two a `while` already has — incrementing at the bottom would need a third place
for a block to land. The bound is re-evaluated each turn, like a `while`
condition, because holding it would need a local and those are only allocated in
the entry block. There is no iteration over a vector: the element type is not
recorded, so `for v in xs` could not say what `v` is.

`xs[i]` reads a vector slot and `xs[i] = v` writes one. A read yields an int,
because nothing at the subscript says otherwise; a text element needs
`vec_get_text(xs, i)`, which says so at the call. A write looks at the value it
was given, so it picks the right store on its own.

A struct value is a heap block of one eight-byte slot per field, wide enough for
an i32 or a pointer, zeroed on allocation. Because the value is itself a pointer
it lives in the same `%name.p` slot as text, so no third slot kind was needed —
only a note of which struct a local holds, kept as `p:Point` in the same bag that
records `p` as pointer-shaped. Declarations emit nothing; field offsets and types
are read back out of the source where a literal or field access needs them.

Struct names must start with a capital. `while i < n {` also puts an identifier
before a brace, so the name is what tells a literal from the head of a loop.
Getting this wrong does not produce a type error — it turns every loop in the
compiler's own source into an allocation, and it broke the fixpoint once before
`bootstrap_guest_loop_head_is_not_a_struct_literal` existed.

Sequences come from the runtime rather than from syntax: `vec_new`, `vec_len`,
`vec_push_int` / `vec_push_text`, `vec_get_*`, `vec_set_*`, `vec_free`
([std.collections](STDLIB.md)). There is no `[...]` literal and no `xs[i]`. That
is deliberate — an index bracket is another place where a token means one thing
in an expression and another in a declaration, and the compiler has no type
information at the point it would have to choose. Calls need no such decision.
A slot holds an int or a text and does not remember which, so push and read the
same kind.

Sets come from the same place and answer membership without scanning: `set_new`,
`set_add` (1 the first time a key arrives, 0 after), `set_has`, `set_add_csv`,
`set_len`, `set_free`. The emitter uses one to remember which locals a function
has already declared. That used to be a comma-separated string, tested by
scanning it and extended by rebuilding it, so declaring n locals cost O(n²) and a
function with a thousand of them took longer to compile than the rest of the file
put together.

Three characters cannot be written as literals in the emitter's own source: `{`
opens a string interpolation, `}` closes one, and `"` ends the string. It used to
obtain each of them by searching its input for an occurrence, which is a scan of
the whole file per function. `char_of(b)` builds a one-character text from a byte
value instead.

The largest cost was not in the emitter at all. `byte(s, i)` measured `s` with
`strlen` on every call, so reading a character cost the length of the file and
reading a file cost its length squared — the compiler spent its time measuring
its input rather than compiling it. The runtime now remembers the lengths of the
strings most recently measured (`buraaq_length_of` in `buraaq_rt.c`), which makes
a read constant.

Compile time is now proportional to input size and no longer to its square:

| statements | before | after |
|-----------:|-------:|------:|
| 400 | 0.18s | 0.026s |
| 1,600 | 3.85s | 0.064s |
| 6,400 | — | 0.212s |

Microseconds per byte fall as the file grows (1.93 → 1.11 → 0.88), which is fixed
startup being amortised rather than any remaining superlinear term. The compiler
emits its own 43,206-line module in 0.064s, down from 2.06s.
`bootstrap_guest_scales_with_input_size` fails if the square ever returns.

It has no lowering for `trait`, `impl`, `spawn`, `defer`, `unsafe`, `extern`,
generics, `[...]` literals or floats. It used to
parse past those and emit references to values it never defined, so the failure
arrived as a clang error about the generated IR (`use of undefined value
'%Point.i'`) rather than as a complaint about the source. `l_check` now refuses
them up front, one diagnostic per line, exit 1:

```
point.bq:1: buraaq-boot cannot lower `struct`
buraaq-boot: the bootstrap compiler handles int/text/bool, if/elif/else, while, fn and calls
```

`bootstrap_guest_rejects_what_it_cannot_lower` pins this down, and also checks
the supported subset still compiles so the test cannot pass by rejecting
everything. Anything added to the emitter must come off the list in `l_unsupported`
in the same change.

**M17** widens that fixpoint to the whole guest. M12 covered `llvm.bq` and
`scan.bq`; `lexer.bq`, `parser.bq`, `names.bq` and `mir.bq` were only checked
against the host on goldens. Now `src/main.bq` — every module, reached through
`use` — is compiled by the guest, the result is compiled again by that, and the
two emit byte-identical IR across 84,966 lines. Each of the five stages of the
guest-built compiler produces the same output as the host-built one on the
golden. `scripts/bootstrap-fixpoint.ps1` runs the same three stages by hand.

M17 did not yet remove rustc from the product path. M21–M24 did: pack, test,
run, and clone proof are guest + clang. The old rustc host CLI is deleted;
`dist/buraaq` is the compiler. Constructs the guest still does not fully lower
(generics, real concurrent spawn, trait-method dispatch) stay on the still-hardening list.

String interpolation takes a name, not an expression: `"{f(x)}"` reads a local
called `f(x)`, which no function defines, and the module fails in clang. Bind the
call to a local first.

## M18 — what the goldens actually cover

Every parity milestone from M3 to M17 was measured on `golden/sample.bq`, which is
nine lines: one `if`, one `return`, one call, one addition. A stage could agree
with the host on that and be wrong about everything else, and two of them were.
`names.bq` held its parameters in `p0` and `p1` and counted them in `np`, so a
function with three parameters reported the third as a function reference; it knew
`if` and `return` and nothing else, so a `while` was read as an expression and the
walker did not terminate. `mir.bq` carried its own copy of the scanner and knew
three operators — `+`, `-`, `>` — out of thirteen. None of this was detectable,
because `sample.bq` contains none of it.

M18 replaces "the stage agrees on one program" with "the stage agrees across the
surface". Two goldens do it:

`golden/tokens.bq` names every token kind the lexer can produce — all 53 keywords,
every operator, and every literal form including char, bytes, floats with
exponents, and digit separators. It is lexed, not compiled; it is not a valid
program, and it does not need to be, because only the kind stream is compared.
That covers 94 of the 96 arms of `TokenKind::golden_name`. The two left out cannot
be reached: the test drops `Eof` from both streams, and no source can produce
`Hash`, because `#` is always consumed as a line comment before a token is cut —
the `'#' => TokenKind::Hash` arm in the host lexer is dead.

`golden/ast.bq` is the other half, and unlike `tokens.bq` it is real code that has
to parse and run: assignment, `while`, `for`, `break`, `continue`, the
`elif`/`else` chain, every binary operator, unary, functions of more than two
parameters, and each literal form the emitter can lower. Three stages are compared
against the host on it — 331 parser events, 194 name events, 244 MIR events — and
a fourth check runs the guest-*built* lexer and parser on both goldens, so a
miscompile of `parser.bq` is caught rather than a disagreement in it. That last
one used to assert only that the dump had more than ten lines in it, which a
parser that had lost an entire statement form would still satisfy.

Events stay in source order, so `a + b * c` and `a * b + c` give the same stream.
These tests say what was recognised and in what order, not what tree was built;
precedence is decided in the emitter and checked by running programs, in the
M15/M16 tests and the fixpoint.

### One scanner

The reason `mir.bq` knew three operators while `parser.bq` knew thirteen is that
each had its own copy of the scanner — `g_skip`, `p_skip` and `scan_skip` were the
same function three times, and the operator table was written twice. Adding an
operator meant finding every copy, and nothing failed when you didn't.

`scan.bq` now holds the one scanner: whitespace and comments, identifiers, keyword
tests, string literals with interpolation, and `scan_op_at`/`scan_op_len`, which
name the operator at a position and say how wide it is. `parser.bq`, `names.bq`
and `mir.bq` all read from it and differ only in what they print — the parser
prints the token's name, the MIR walker prints the instruction it lowers to, the
name table prints nothing for operators at all.

### How a local is known to be one

The name table classifies each identifier as a builtin, a local, or a reference to
a function, which requires knowing what is in scope. Parameters used to be the
whole answer, held in two slots. Locals now live in a set that grows as bindings
are met: a parameter, an assignment, or a `for` variable adds a name, and it
reports as a local from there on. A binding also reports where it is written,
which is what makes `total = total + i` two mentions of one local rather than an
unknown name followed by a local.

Two places needed a rule rather than a guess. `name = expr` binds but `name ==
expr` compares, so the byte after `=` decides. And `p.x` is a field while `a..b`
is a range, so a `.` is only a field when an identifier starts right after it —
without that, the left bound of a range is read as the base of a field access and
the walker consumes the wrong span. `parser.bq` had the same latent bug, unhit
only because `ast.bq` writes `0..n` and the digit path returns before the check.

### Entry points instead of a weave

The M9 tests used to build their unit by pasting a module's text in front of a
generated `main`. That is the scaffolding M13 removed for `llvm.bq`, and it
survived here until `parser.bq` imported the shared scanner — a concatenated file
carries no `use` resolution, so the guest-built parser stopped linking. There are
now `boot_lexer.bq` and `boot_parser.bq` beside `boot_llvm.bq`, each a real entry
point that reaches its stage through `use`. They live outside `src/` so the host
does not compile them as a second `main` alongside `src/main.bq`.

### A call and its definition agree about what comes back

Widening the walkers turned up a bug the fixpoint could not see. The emitter reads
a definition's return type from the signature but a call's from `l_rt`, a list of
names, and `n_bind` was void and not on the list — so it was defined `void` and
called as `i32`, and the caller read a register the callee never set. clang accepts
that and the value was always discarded, so nothing failed. The fixpoint in
particular could not catch it: both stages emitted the same wrong IR, and equal is
all it asks for. `bootstrap_guest_ir_agrees_on_return_types` now pairs every
definition in the whole-guest module against every call of it. The list is still a
list, but disagreeing with it is no longer silent.

The whole-guest fixpoint still holds with the frontend at full width: stage1 and
stage2 emit byte-identical IR across 84,966 lines, up from 64,930 before the
walkers grew.

One thing M18 does not claim is that the guest agrees with the host about
*meaning*. These are four streams of events from four walks over the same text.
The name table says `total` is a local; it does not say what type it holds.

## M19 — types from signatures

A call used to ask `l_rt`, a list of names inside the emitter, what came back.
A function missing from that list was an int, even when it was written
`-> text`, and a void function missing from it was defined `void` and called as
`i32`. The list had to mention every function in the compiler, and adding one
meant checking it. `bootstrap_guest_ir_agrees_on_return_types` caught the
disagreement after the fact; it could not say what the type *should* have been.

`l_collect_sigs` walks the unit once and records each `fn name(...) -> T` as
void, int or text. Runtime builtins that are not `fn` in the source (print,
concat, the vector helpers) seed a second bag, concatenated behind, so a
user function of the same name wins. `l_rt` is a lookup in that bag. A golden
that defines `fn wave() -> text` and `fn hush()`, names that were never on the
old lists, must compile, call them as `i8*` and `void`, and print `hello-m19`.

Diagnostics from `l_check` run on each file as it is read, with that file's
path and its own line numbers. An unsupported construct in an imported module
used to be reported against the entry file, at a line that only existed after
the weave. `bootstrap_m19_diagnostics_name_the_source_file` puts `spawn` in
`leaf.bq` and requires the diagnostic to name `leaf.bq`.

Locals are still classified by assignment (`l_text_locals`), not by a separate
type checker: `s = wave()` is text because `wave` is. Ownership and borrows in
the product compiler are the guest subset; the old rustc type checker is gone.

## M20 — rustc is not required to rebuild the guest

M17 already rebuilt `compiler-buraaq` with the guest. The test process still had
cargo and rustc on PATH, so a stray spawn would have succeeded. M20 gives the
guest a PATH that contains clang and the OS directories and does not contain
rustc or cargo, then requires stage1 and stage2 to emit byte-identical IR. The
hardcoded compiler-function list in `l_rt` must stay gone: if it returns, the
test fails on the source, not on a miscompile that the fixpoint would bless.

At M20, rustc still built a separate host CLI (package manager, Ship, LSP).
That tree is gone. `dist/buraaq` is the guest (M21). Users install that binary
(M10). Compiler developers rebuilding `compiler-buraaq` use clang plus a guest
binary, which is what `buraaq doctor` prints as `bootstrap:`.

**M10** is install/doctor finding clang via PATH, `BURAAQ_CLANG`, or `%LOCALAPPDATA%\buraaq\llvm` (not a 400MB LLVM tree in git). User install copies `dist/buraaq` and does not invoke Cargo.

## M21 — the product compiler is the guest

M20 proved the guest can rebuild itself with rustc off PATH. That still left
`dist/buraaq` as a rustc-built host binary, and `scripts/pack-dist` still
spawned cargo. That is not beating rustc: it is hiding rustc behind a script.

M21 makes the guest the compiler you install. `buraaq run FILE.bq` finds clang
and the runtime, emits IR, links, and executes — no extra paths from the
caller, rustc not on PATH. `buraaq build FILE [OUT]` writes a native binary.
`buraaq doctor` prints clang, `buraaq_rt.c`, and `self-hosted`.
`scripts/pack-dist.ps1` / `.sh` rebuild that guest with clang; they do not
contain `cargo build`. Stage0 is a previous guest binary or `boot/stage0.ll`.
They do not fall back to a rustc-built host.

The first stage0 on a clean machine is `compiler-buraaq/boot/stage0.ll` plus
clang. After that, every rebuild is guest-on-guest. That is the same chicken-egg
gcc has; the difference is the product binary is no longer rustc's.

## M22 — the install path is a project

M21 made `buraaq run FILE` the product. Users still type `buraaq new hello --cli`
then `buraaq run` in that directory. M22 is that path on the guest: `new` writes
`buraaq.pkg` and a `module main` hello, `-C` selects the project, `run` and
`build` with no file discover `src/main.bq`, and `build` writes
`target/debug/<name>.exe`.

The same milestone takes `module`, `loop`, and `unsafe` off the refuse list.
`module` is a line skipped like `use`. `loop { }` is `while true` with the
labels `break` already jumps to. `unsafe { }` is a scope; the guest still has
no raw pointers. `extern c { ... }` is skipped so a file that wraps runtime
calls can load. Generics stay on the still-hardening list.

## M23 — rustc is gone from the product path

M22 still refused `const`, `continue`, `defer`, and `where`, so a file that used
them could not be the proof that rustc is gone. M23 lowers integer `const` /
`static` into every function, `continue` as the loop back-edge, and `defer` as a
LIFO flush on `return` and on falling off the function. `type`, `import`, and
`where` are skipped. `buraaq test` runs `selftest/main.bq` (or a file you pass).
`scripts/selfhost-test.ps1` / `.sh` rebuild the guest with clang and run that
selftest with rustc off PATH.

## M24 — a clone does not need rustc

M23 still seeded CI with `cargo build -p buraaq` and refused `spawn` / `trait` /
`impl` / `async` / `await`, so a fresh clone with only clang could not prove the
compiler. M24 inlines `spawn { }` (and `spawn f()`) as the body's statements,
skips `trait` braces, unwraps `impl` so the methods are ordinary functions,
strips `async` / `await`, and commits `compiler-buraaq/boot/stage0.ll`. CI's
`selfhost` job has no rust-toolchain: clang links that IR, then
`scripts/selfhost-test` rebuilds the guest and runs `buraaq test`. Pack and
self-host seed from that IR or a previous guest binary; they do not fall back
to a rustc-built host.

## Rules

1. **The guest is the product.** Pack `dist/buraaq` from `compiler-buraaq/` + clang.
2. **Subset first.** M4–M8 cover the golden (and later `lexer.bq` itself). Not the whole language.
3. **No fake self-host.** M9 is not “we have `.bq` files.” It is “a Buraaq-built LLVM path compiles the lexer/parser and goldens still pass.” M21 is not “we rebuilt llvm.bq.” It is “`buraaq run` of the golden prints `5` with rustc off PATH, and pack-dist does not contain cargo.” M23 is “`buraaq test` of `selftest/main.bq` prints `413489` with rustc off PATH.” M24 is “CI `selfhost` has no rust-toolchain; clang links `boot/stage0.ll` and the guest rebuilds itself.”
4. **LLVM is the remaining native dependency after M8.** Pack it (M10). Do not pack Rust.

## M10 layout (install)

```
%LOCALAPPDATA%\buraaq\   or  ~/.local/share/buraaq/
  bin/buraaq.exe
  llvm/bin/clang.exe     (fetched or copied, not committed)
```

`buraaq doctor` prints clang, the runtime, that this binary is self-hosted, and
that scripting compiles a temp `.bq` with clang.

## After M26

`buraaq` with no args is an interactive shell. `-e` and `script FILE.bq` compile
the same language through clang (wrap a snippet in `fn main` when needed).
Stdin is `read("-")` / `buraaq_read_line` in the C runtime.

## Not this track

Gate D (7-day fuzz), registry, DAP, channels. Those stay on the 1.0 gate list.
