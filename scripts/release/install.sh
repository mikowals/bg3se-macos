#!/bin/bash
#
# BG3SE-macOS prebuilt installer (ships inside the release zip).
#
# Copies libbg3se.dylib into the game bundle and adds an LC_LOAD_WEAK_DYLIB
# load command to the game binary with insert_dylib, exactly as
# `bg3se-harness patch` does: backup as "<binary>.bg3se-original", patch in
# place, ad-hoc re-sign. Safe to re-run; a game update is detected because the
# fresh binary no longer links libbg3se, and the stale backup is replaced.
#
# Usage:
#   ./install.sh              # find the game in the Steam libraries
#   BG3SE_GAME_PATH=/path/to/"Baldur's Gate 3.app" ./install.sh
#   ./install.sh --uninstall  # restore the original binary, remove the dylib
#
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
DYLIB="$HERE/libbg3se.dylib"
INSERT_DYLIB="$HERE/insert_dylib_bin"
INSTALL_NAME="@loader_path/libbg3se.dylib"
BACKUP_SUFFIX=".bg3se-original"

find_bg3_app() {
    local app_names=("Baldur's Gate 3.app" "Baldurs Gate 3.app")
    local name dir

    if [[ -n "${BG3SE_GAME_PATH:-}" ]]; then
        if [[ "$BG3SE_GAME_PATH" == *.app && -d "$BG3SE_GAME_PATH" ]]; then
            echo "$BG3SE_GAME_PATH"; return 0
        fi
        for name in "${app_names[@]}"; do
            [[ -d "$BG3SE_GAME_PATH/$name" ]] && { echo "$BG3SE_GAME_PATH/$name"; return 0; }
        done
        echo "BG3SE_GAME_PATH is set but no BG3 app bundle is there: $BG3SE_GAME_PATH" >&2
    fi

    local steam="$HOME/Library/Application Support/Steam"
    local candidates=("$steam/steamapps/common/Baldurs Gate 3")
    local vdf="$steam/steamapps/libraryfolders.vdf"
    if [[ -f "$vdf" ]]; then
        local root
        while IFS= read -r root; do
            candidates+=("$root/steamapps/common/Baldurs Gate 3")
        done < <(grep -o '"path"[[:space:]]*"[^"]*"' "$vdf" | sed 's/.*"path"[[:space:]]*"//; s/"$//')
    fi
    for dir in "${candidates[@]}"; do
        for name in "${app_names[@]}"; do
            [[ -d "$dir/$name" ]] && { echo "$dir/$name"; return 0; }
        done
    done
    return 1
}

APP="$(find_bg3_app)" || {
    echo "Baldur's Gate 3.app not found. Set BG3SE_GAME_PATH to the .app bundle." >&2
    exit 1
}
MACOS_DIR="$APP/Contents/MacOS"
EXE="$MACOS_DIR/Baldur's Gate 3"
BACKUP="$EXE$BACKUP_SUFFIX"

if pgrep -x "Baldur's Gate 3" >/dev/null 2>&1; then
    echo "Baldur's Gate 3 is running. Quit it first." >&2
    exit 1
fi

is_patched() { otool -L "$EXE" 2>/dev/null | grep -q libbg3se; }

if [[ "${1:-}" == "--uninstall" ]]; then
    if [[ -f "$BACKUP" ]]; then
        mv -f "$BACKUP" "$EXE"
        echo "Restored original binary from $BACKUP"
    elif is_patched; then
        echo "No backup at $BACKUP; verify the game files through Steam to restore the binary." >&2
        exit 1
    fi
    rm -f "$MACOS_DIR/libbg3se.dylib" "$MACOS_DIR/.bg3se-patch-hash"
    echo "Removed libbg3se.dylib"
    exit 0
fi

[[ -f "$DYLIB" ]] || { echo "libbg3se.dylib not found next to this script" >&2; exit 1; }
[[ -x "$INSERT_DYLIB" ]] || { echo "insert_dylib_bin not found next to this script" >&2; exit 1; }

# The game updated: the fresh binary is unpatched, so the old backup is stale.
if [[ -f "$BACKUP" ]] && ! is_patched; then
    rm -f "$BACKUP"
fi

cp -f "$DYLIB" "$MACOS_DIR/libbg3se.dylib"
echo "Installed $(du -h "$MACOS_DIR/libbg3se.dylib" | cut -f1) libbg3se.dylib -> $MACOS_DIR"

if is_patched; then
    echo "Game binary already links libbg3se; patch step skipped."
else
    [[ -f "$BACKUP" ]] || cp -p "$EXE" "$BACKUP"
    if ! "$INSERT_DYLIB" --weak --inplace --all-yes "$INSTALL_NAME" "$EXE" >/dev/null 2>&1; then
        "$INSERT_DYLIB" --weak --inplace --strip-codesig --all-yes "$INSTALL_NAME" "$EXE"
    fi
    codesign --deep -f -s - "$EXE" >/dev/null 2>&1 || {
        echo "codesign failed; the game may refuse to launch. Restore with: $0 --uninstall" >&2
        exit 1
    }
    is_patched || { echo "Patch verification failed (otool does not list libbg3se)" >&2; exit 1; }
    echo "Patched game binary (backup: $BACKUP)"
fi

# Skip the Larian launcher, as the harness does.
defaults write com.larian.bg3 NoLauncher 1 >/dev/null 2>&1 || true

echo "Done. Launch through Steam; logs: ~/Library/Application Support/BG3SE/logs/latest.log"
