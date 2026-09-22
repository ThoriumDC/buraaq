#!/usr/bin/env bash
# Prove the product CLI.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RT="$ROOT/stdlib/runtime/buraaq_rt.c"
STD="$ROOT/stdlib/runtime/buraaq_std.c"
GOLDEN="$ROOT/compiler-buraaq/golden/sample.bq"
SELFTEST="$ROOT/compiler-buraaq/selftest/main.bq"

clang_bin="${BURAAQ_CLANG:-}"
if [ -z "$clang_bin" ] || [ ! -x "$clang_bin" ]; then
  if [ -x "$HOME/.local/share/buraaq/llvm/bin/clang" ]; then
    clang_bin="$HOME/.local/share/buraaq/llvm/bin/clang"
  else
    clang_bin="clang"
  fi
fi

hide_rustc() {
  clang_dir="$(dirname "$(command -v "$clang_bin" 2>/dev/null || echo /usr/bin/clang)")"
  export PATH="$clang_dir:/usr/lib/llvm-20/bin:/usr/lib/llvm-18/bin:/usr/bin:/bin:${PATH:-}"
  unset CARGO RUSTC CARGO_HOME RUSTUP_HOME || true
}

work="${TMPDIR:-/tmp}/bq-selfhost-verify"
mkdir -p "$work"

link_guest() {
  local ir="$1"
  local exe="$2"
  local extra=()
  if command -v ld.lld >/dev/null 2>&1; then
    extra+=(-fuse-ld=lld)
  fi
  "$clang_bin" -Wno-override-module -Wno-deprecated-declarations -O0 \
    "${extra[@]}" -o "$exe" "$ir" "$RT" "$STD" -lpthread -lm
  chmod +x "$exe"
}

find_compiler() {
  local c ver
  for c in \
    "$ROOT/dist/buraaq" \
    "$ROOT/dist/buraaq.exe" \
    "$ROOT/compiler-buraaq/target/debug/buraaq-compiler" \
    "$ROOT/compiler-buraaq/target/debug/buraaq-compiler.exe"; do
    case "$c" in *.exe|*.EXE) continue ;; esac
    if [ -x "$c" ]; then
      ver="$("$c" --version 2>/dev/null || true)"
      if echo "$ver" | grep -q "sysroot: MISSING"; then continue; fi
      if echo "$ver" | grep -q "self-hosted"; then
        echo "$c"
        return 0
      fi
    fi
  done
  if [ -f "$ROOT/compiler-buraaq/boot/stage0.ll" ]; then
    link_guest "$ROOT/compiler-buraaq/boot/stage0.ll" "$work/stage0-seed"
    echo "$work/stage0-seed"
    return 0
  fi
  echo "selfhost-verify needs a self-hosted Buraaq compiler or compiler-buraaq/boot/stage0.ll." >&2
  exit 1
}

assert_printed() {
  local got="$1"
  local want="$2"
  local label="$3"
  got="$(printf '%s' "$got" | tr -d '[:space:]')"
  if [ "$got" != "$want" ]; then
    echo "$label want $want got '$got'" >&2
    exit 1
  fi
  echo "PASS $label -> $want"
}

hide_rustc
bq="$(find_compiler)"
ver="$("$bq" --version 2>/dev/null || true)"
if ! echo "$ver" | grep -q "self-hosted"; then
  echo "compiler --version must contain self-hosted, got '$ver'" >&2
  exit 1
fi
echo "PASS version self-hosted"
"$bq" doctor >/dev/null
echo "PASS doctor"

export BURAAQ_RUN_OUT="$work/golden-run"
out="$("$bq" run "$GOLDEN")"
assert_printed "$out" "5" "golden/sample.bq"

export BURAAQ_RUN_OUT="$work/selftest-run"
out="$("$bq" test "$SELFTEST")"
assert_printed "$out" "413489" "selftest"

rm -rf "$work/hello"
"$bq" -C "$work" new hello --cli >/dev/null
export BURAAQ_RUN_OUT="$work/hello-run"
"$bq" -C "$work/hello" run >/dev/null
echo "PASS new --cli then run"

write_and_run() {
  local name="$1"
  local want="$2"
  local src="$3"
  printf '%s' "$src" > "$work/$name.bq"
  export BURAAQ_RUN_OUT="$work/$name-run"
  local printed
  printed="$("$bq" run "$work/$name.bq")"
  assert_printed "$printed" "$want" "stress $name"
}

write_and_run add 5 $'fn add(a: int, b: int) -> int { a + b }\nfn main() { print(add(2, 3)) }\n'
write_and_run loop 10 $'fn main() {\n    mut n = 0\n    mut k = 0\n    while k < 5 {\n        n = n + k\n        k = k + 1\n    }\n    print(n)\n}\n'
write_and_run match 2 $'enum Color {\n    Red\n    Blue\n}\nfn main() {\n    c = Color.Blue\n    match c {\n        Color.Red => {\n            print(1)\n        }\n        Color.Blue => {\n            print(2)\n        }\n    }\n}\n'
write_and_run defer 19 $'fn main() {\n    defer print(9)\n    print(1)\n}\n'
write_and_run spawn 24 $'fn tick(n: int) -> int { n + 1 }\nfn main() {\n    spawn {\n        print(2)\n    }\n    print(tick(3))\n}\n'

export BURAAQ_RUN_OUT="$work/print-stress-run"
printed="$("$bq" run "$ROOT/examples/release-gate/print_stress.bq")"
echo "$printed" | grep -q "hello" || { echo "print_stress missing hello" >&2; exit 1; }
echo "$printed" | grep -q "1.5" || { echo "print_stress missing 1.5" >&2; exit 1; }
echo "$printed" | grep -q "mix 7 2.5 false" || { echo "print_stress missing mix" >&2; exit 1; }
echo "$printed" | grep -q "abcd" || { echo "print_stress missing abcd" >&2; exit 1; }
echo "PASS print_stress"

echo "selfhost-verify: product CLI, golden, selftest, and guest stress passed."
