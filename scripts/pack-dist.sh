#!/usr/bin/env bash
# Pack the self-hosted compiler into dist/.
# Stage0 is a previous guest binary or clang-linked compiler-buraaq/boot/stage0.ll.
# The artifact is always the guest: Buraaq compiling Buraaq, linked with clang.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST="$ROOT/dist"
GUEST_SRC="$ROOT/compiler-buraaq/src/main.bq"
RT="$ROOT/stdlib/runtime/buraaq_rt.c"
STD="$ROOT/stdlib/runtime/buraaq_std.c"
OUT="$DIST/buraaq"
mkdir -p "$DIST"

clang_bin="${BURAAQ_CLANG:-}"
if [ -z "$clang_bin" ] || [ ! -x "$clang_bin" ]; then
  if [ -x "$HOME/.local/share/buraaq/llvm/bin/clang" ]; then
    clang_bin="$HOME/.local/share/buraaq/llvm/bin/clang"
  else
    clang_bin="clang"
  fi
fi

link_guest() {
  local ir="$1"
  local exe="$2"
  "$clang_bin" -Wno-override-module -Wno-deprecated-declarations -O2 -fuse-ld=lld \
    -o "$exe" "$ir" "$RT" "$STD" -lpthread -lm
  chmod +x "$exe"
}

work="${TMPDIR:-/tmp}/bq-pack-dist"
mkdir -p "$work"

stage0=""
for c in \
  "$ROOT/compiler-buraaq/target/debug/buraaq-compiler" \
  "$ROOT/compiler-buraaq/target/debug/buraaq-compiler.exe" \
  "$DIST/buraaq" \
  "$DIST/buraaq.exe"; do
  if [ -x "$c" ]; then
    ver="$("$c" --version 2>/dev/null || true)"
    if echo "$ver" | grep -q "sysroot: MISSING"; then continue; fi
    if echo "$ver" | grep -q "self-hosted"; then
      stage0="$c"
      break
    fi
  fi
done
if [ -z "$stage0" ] && [ -f "$ROOT/compiler-buraaq/boot/stage0.ll" ]; then
  link_guest "$ROOT/compiler-buraaq/boot/stage0.ll" "$work/stage0-seed"
  stage0="$work/stage0-seed"
fi
if [ -z "$stage0" ]; then
  echo "pack-dist needs a self-hosted Buraaq compiler or compiler-buraaq/boot/stage0.ll." >&2
  exit 1
fi

copy_sysroot() {
  local sys="$DIST/sysroot"
  mkdir -p "$sys/runtime"
  if [ -d "$ROOT/stdlib/src" ]; then
    rm -rf "$sys/src"
    cp -R "$ROOT/stdlib/src" "$sys/src"
  fi
  if [ -d "$ROOT/stdlib/runtime" ]; then
    rm -rf "$sys/runtime"
    cp -R "$ROOT/stdlib/runtime" "$sys/runtime"
  fi
  cp "$RT" "$sys/runtime/buraaq_rt.c"
  if [ -f "$ROOT/stdlib/buraaq.pkg" ]; then
    cp "$ROOT/stdlib/buraaq.pkg" "$sys/buraaq.pkg"
  fi
}

ir="$work/guest.ll"
"$stage0" llvm "$GUEST_SRC" > "$ir"
link_guest "$ir" "$OUT"
copy_sysroot

mkdir -p "$ROOT/compiler-buraaq/boot"
cp "$ir" "$ROOT/compiler-buraaq/boot/stage0.ll"

echo "Packed $OUT (self-hosted guest)"
echo "Users install with: ./install.sh"
