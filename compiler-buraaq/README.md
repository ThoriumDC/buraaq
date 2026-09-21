Compiler **written in Buraaq**. This is the product compiler. Users install `dist/buraaq` and never open this folder. Track: [docs/BOOTSTRAP.md](../docs/BOOTSTRAP.md).

| Milestone | Status | Evidence |
|-----------|--------|----------|
| **M3–M8** Lexer through guest clang | **PASS** | goldens + `print(add(2,3))` is `5` |
| **M9** Guest lexer/parser | **PASS** | guest LLVM compiles `lexer.bq`/`parser.bq` |
| **M10** Install sidecar | **PASS** | clang via PATH / `BURAAQ_CLANG` / sidecar; install copies `dist/` |
| **M12–M17** Fixpoint, modules, enums, loops, whole guest | **PASS** | stage1 and stage2 emit byte-identical IR |
| **M18** Full-surface parity | **PASS** | `tokens.bq` / `ast.bq` |
| **M19** Types from signatures | **PASS** | call `->` types and per-file diagnostics |
| **M20** rustc-off of this package | **PASS** | guest rebuilds itself with clang |
| **M21** Product compiler | **PASS** | `buraaq run golden/sample.bq` prints `5`; pack-dist does not invoke cargo |
| **M22** Product path | **PASS** | `buraaq new hello --cli` then `buraaq run`; `module` / `loop` / `unsafe` |
| **M23** rustc-free proof | **PASS** | `buraaq test` selftest; `scripts/selfhost-test` |
| **M24** rustc-free clone | **PASS** | `boot/stage0.ll`; spawn inlined; trait/impl/async skipped |
| **M25** mut / float / print | **PASS** | `mut` locals; float literals; typed and multi-arg print |
| **M26** stdlib runtime | **PASS** | runtime in stdlib; CI is `selfhost` |

## Run

Clone + clang:

```text
powershell -File scripts/selfhost-test.ps1
powershell -File scripts/selfhost-verify.ps1
# or: bash scripts/selfhost-test.sh && bash scripts/selfhost-verify.sh
```

```text
buraaq new hello --cli
cd hello
buraaq run
buraaq build
buraaq doctor
buraaq test
buraaq -e "print_int(40+2)"
```

Dump commands:

```bash
buraaq run -C compiler-buraaq -- golden/sample.bq
buraaq run -C compiler-buraaq -- parse golden/sample.bq
buraaq run -C compiler-buraaq -- names golden/ast.bq
buraaq run -C compiler-buraaq -- mir golden/ast.bq
buraaq run -C compiler-buraaq -- llvm golden/sample.bq
```

Rebuilding, testing, and packing this package is clang plus a guest binary or `boot/stage0.ll`.
