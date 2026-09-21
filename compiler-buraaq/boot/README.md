# Stage0 seed

`stage0.ll` is the clone seed: LLVM IR of the guest compiler (~1.9MB).

A clone with clang links this file, then `scripts/selfhost-test` rebuilds the
guest and runs `buraaq test`.

```text
powershell -File scripts/selfhost-test.ps1
powershell -File scripts/selfhost-verify.ps1
```

Regenerate by running `scripts/selfhost-test`. The script overwrites this file
with stage1 IR after the selftest prints `413489`.
