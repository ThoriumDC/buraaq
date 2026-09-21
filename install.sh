#!/usr/bin/env bash
# Install the Buraaq compiler onto PATH (POSIX).
# One step. Prefers packaged dist/buraaq.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${1:-$HOME/.local/bin}"
mkdir -p "$PREFIX"
SRC=""
if [ -x "$ROOT/dist/buraaq" ]; then
  SRC="$ROOT/dist/buraaq"
fi
if [ -z "$SRC" ]; then
  echo "Packaged compiler missing (dist/buraaq)." >&2
  echo "Get a packaged tree, or on a packager machine run: ./scripts/pack-dist.sh" >&2
  exit 1
fi
echo "Using packaged compiler $SRC"
cp "$SRC" "$PREFIX/buraaq"
chmod +x "$PREFIX/buraaq"

# discover_sysroot looks next to the binary: $PREFIX/sysroot/src/io.bq
SYS="$PREFIX/sysroot"
STDLIB="$ROOT/stdlib"
if [ ! -f "$STDLIB/src/io.bq" ]; then
  echo "stdlib missing under $STDLIB (needed next to the installed compiler)" >&2
  exit 1
fi
mkdir -p "$SYS"
for part in src runtime; do
  rm -rf "$SYS/$part"
  cp -R "$STDLIB/$part" "$SYS/$part"
done
if [ -f "$STDLIB/buraaq.pkg" ]; then
  cp "$STDLIB/buraaq.pkg" "$SYS/buraaq.pkg"
fi
echo "Installed sysroot $SYS"
if [ -x "$ROOT/scripts/ensure-llvm.sh" ]; then
  "$ROOT/scripts/ensure-llvm.sh" || true
fi
echo "Installed $PREFIX/buraaq"
echo "Add $PREFIX to PATH if buraaq is not found."
echo "Then run: buraaq doctor"
echo "         buraaq run FILE.bq"
echo "Rust was not required."
