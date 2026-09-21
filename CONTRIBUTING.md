# Contributing to Buraaq

Thank you for helping make Buraaq production-ready.

## Getting started

1. Clone the repository
2. Install clang, then `powershell -File scripts/selfhost-test.ps1` and `powershell -File scripts/selfhost-verify.ps1`
3. Read `docs/INTERNALS.md` and `docs/COMPILER_ARCHITECTURE.md`

## Change guidelines

- **Minimal diffs** — match existing style in each file
- **Diagnostics first** — user-facing errors need stable IDs, spans, and help text
- **Measure optimizations** — update benchmarks; no blind `-O3`
- **No silent unsafe** — document FFI and `unsafe` escape hatches

## Pull request checklist

- [ ] `scripts/selfhost-test` and `scripts/selfhost-verify` pass
- [ ] Product compiler stays Buraaq (`compiler-buraaq/` + clang)
- [ ] New syntax has a golden or selftest that fails if it drifts
- [ ] User-visible behavior documented in `docs/` or CHANGELOG

## Running benchmarks

Use `dist/buraaq` (or `scripts/pack-dist.ps1` first).

```powershell
cd benchmarks
.\run_quick.ps1 -Release        # integer_sum / fib_iter / numerical_loop
.\run_suite.ps1 -Release        # -O2 vs C++
.\run_suite.ps1 -ReleaseFast    # -O3 + thin LTO
```

Report unfavorable results honestly in PR descriptions.

## Tests

Product proof is the self-host chain:

```powershell
powershell -File scripts/selfhost-test.ps1
powershell -File scripts/selfhost-verify.ps1
```

`selfhost-verify` covers version, doctor, goldens, selftest, `new --cli`, mut/match/defer/spawn, and print stress.

## Code of conduct

Be direct, technical, and respectful. Debate design in issues/ADRs, not personal terms.
