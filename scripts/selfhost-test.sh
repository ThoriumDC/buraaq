#!/usr/bin/env bash
# Prove the product compiler rebuilds itself.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GUEST_SRC="$ROOT/compiler-buraaq/src/main.bq"
SELFTEST="$ROOT/compiler-buraaq/selftest/main.bq"
RT="$ROOT/stdlib/runtime/buraaq_rt.c"
STD="$ROOT/stdlib/runtime/buraaq_std.c"
WANT="413489"

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

work="${TMPDIR:-/tmp}/bq-selfhost-test"
mkdir -p "$work"

dedupe_ll_declares() {
  local ir="$1"
  local tmp="$ir.dedupe"
  awk '!/^declare / || !seen[$0]++' "$ir" > "$tmp"
  mv "$tmp" "$ir"
}

link_guest() {
  local ir="$1"
  local exe="$2"
  case "$ir" in *.ll) dedupe_ll_declares "$ir" ;; esac
  local extra=()
  if command -v ld.lld >/dev/null 2>&1; then
    extra+=(-fuse-ld=lld)
  fi
  "$clang_bin" -Wno-override-module -Wno-deprecated-declarations -O0 \
    "${extra[@]}" -o "$exe" "$ir" "$RT" "$STD" -lpthread -lm
  chmod +x "$exe"
}

find_stage0() {
  local c ver
  for c in \
    "$ROOT/compiler-buraaq/target/debug/buraaq-compiler" \
    "$ROOT/compiler-buraaq/target/debug/buraaq-compiler.exe" \
    "$ROOT/dist/buraaq" \
    "$ROOT/dist/buraaq.exe"; do
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
  echo "selfhost-test needs a self-hosted Buraaq compiler or compiler-buraaq/boot/stage0.ll." >&2
  exit 1
}

assert_selftest() {
  local exe="$1"
  local label="$2"
  export BURAAQ_RUN_OUT="$work/selftest-$label"
  local out
  out="$("$exe" test "$SELFTEST")"
  out="$(printf '%s' "$out" | tr -d '[:space:]')"
  if [ "$out" != "$WANT" ]; then
    echo "$label stdout want $WANT got '$out'" >&2
    exit 1
  fi
  echo "PASS $label selftest -> $WANT"
}

echo "selfhost-test: clang=$clang_bin ($(command -v "$clang_bin" || echo missing)) ld.lld=$(command -v ld.lld || echo none)"
hide_rustc
stage0="$(find_stage0)"
echo "selfhost-test: stage0=$stage0"
product="$work/product"
"$stage0" llvm "$GUEST_SRC" > "$work/guest.ll"
link_guest "$work/guest.ll" "$product"

assert_selftest "$product" "product"

"$product" llvm "$GUEST_SRC" > "$work/guest2.ll"
link_guest "$work/guest2.ll" "$work/stage1"
assert_selftest "$work/stage1" "stage1"

mkdir -p "$ROOT/compiler-buraaq/boot"
cp "$work/guest2.ll" "$ROOT/compiler-buraaq/boot/stage0.ll"

echo "selfhost-test: guest rebuilt itself and passed selftest."
