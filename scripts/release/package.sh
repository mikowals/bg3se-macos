#!/bin/bash
#
# Build the release zip for a GitHub release.
#
#   scripts/release/package.sh [version]     # default: BG3SE_VERSION from src/core/version.h
#
# Output: build/release/bg3se-macos-v<version>-universal.zip containing
#   libbg3se.dylib     universal (arm64 + x86_64) build from build/lib/
#   insert_dylib_bin   the vendored Mach-O patcher (universal)
#   install.sh         standalone installer (copies, patches, re-signs)
#   README.txt         install and uninstall steps
#
# Refuses to package a dylib whose embedded version differs from the requested
# one, or one that is not a universal binary.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DYLIB="$PROJECT_DIR/build/lib/libbg3se.dylib"
INSERT_DYLIB="$PROJECT_DIR/tools/vendor/insert_dylib/insert_dylib_bin"
HEADER_VERSION="$(sed -n 's/^#define BG3SE_VERSION "\(.*\)"/\1/p' "$PROJECT_DIR/src/core/version.h")"
VERSION="${1:-$HEADER_VERSION}"

[[ -f "$DYLIB" ]] || { echo "Build first: cmake --build build ($DYLIB missing)" >&2; exit 1; }
[[ -x "$INSERT_DYLIB" ]] || { echo "insert_dylib_bin missing at $INSERT_DYLIB" >&2; exit 1; }
[[ "$VERSION" == "$HEADER_VERSION" ]] || {
    echo "Requested $VERSION but src/core/version.h says $HEADER_VERSION" >&2; exit 1; }

ARCHS="$(lipo -archs "$DYLIB")"
[[ "$ARCHS" == *arm64* && "$ARCHS" == *x86_64* ]] || {
    echo "libbg3se.dylib is not universal (archs: $ARCHS)" >&2; exit 1; }
# grep -c consumes all input: grep -q would close the pipe early and trip pipefail.
[[ "$(strings -a -arch arm64 "$DYLIB" | grep -cF "$VERSION")" -gt 0 ]] || {
    echo "libbg3se.dylib does not embed version string $VERSION" >&2; exit 1; }

OUT_DIR="$PROJECT_DIR/build/release"
STAGE="$OUT_DIR/bg3se-macos-v$VERSION"
ZIP="$OUT_DIR/bg3se-macos-v$VERSION-universal.zip"
rm -rf "$STAGE" "$ZIP"
mkdir -p "$STAGE"

cp "$DYLIB" "$STAGE/libbg3se.dylib"
cp "$INSERT_DYLIB" "$STAGE/insert_dylib_bin"
cp "$SCRIPT_DIR/install.sh" "$STAGE/install.sh"
chmod +x "$STAGE/install.sh" "$STAGE/insert_dylib_bin"

cat > "$STAGE/README.txt" <<EOF
BG3SE-macOS v$VERSION — Script Extender for Baldur's Gate 3 on macOS
https://github.com/tdimino/bg3se-macos

Install
  1. Quit Baldur's Gate 3.
  2. ./install.sh
     (set BG3SE_GAME_PATH=/path/to/"Baldur's Gate 3.app" if the game is not in a Steam library)
  3. Launch through Steam. Logs: ~/Library/Application Support/BG3SE/logs/latest.log

Uninstall
  ./install.sh --uninstall

What install.sh does
  - copies libbg3se.dylib into Baldur's Gate 3.app/Contents/MacOS/
  - backs up the game binary as "Baldur's Gate 3.bg3se-original"
  - adds an LC_LOAD_WEAK_DYLIB load command with insert_dylib and ad-hoc re-signs
  - sets "defaults write com.larian.bg3 NoLauncher 1" to skip the Larian launcher

After a game update Steam replaces the binary; run ./install.sh again.

Apple Silicon only for this installer: insert_dylib_bin is arm64. On an Intel
Mac build from source (cmake -B build && cmake --build build), which patches
the game automatically; the dylib itself is universal.
EOF

(cd "$OUT_DIR" && zip -qr "$(basename "$ZIP")" "$(basename "$STAGE")")
shasum -a 256 "$ZIP" | tee "$ZIP.sha256"
ls -lh "$ZIP"
