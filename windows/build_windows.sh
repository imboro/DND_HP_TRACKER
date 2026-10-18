#!/bin/bash
# Builds the Windows version of the tracker on a Mac (cross-compiling with
# llvm-mingw) and packages it, separate from the Mac build:
#
#   windows/build_windows.sh
#
#   windows/deps/   the llvm-mingw compiler and SDL2 for Windows (downloaded once)
#   windows/build/  CMake build tree
#   windows/dist/   "HP Tracker/" (exe + SDL2.dll + README.txt) and a zip
#
# Needs: cmake (brew install cmake). Everything else is downloaded.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
# Use the Homebrew-in-home-directory install if present.
export PATH="$HOME/homebrew/bin:$PATH"

LLVM_MINGW_VERSION=20260908
LLVM_MINGW_SHA256=d1dc5d1ecf3a3ced5ed5544c72f1acd0c8e84eb3024d520ecc6b143eec62a149
SDL_VERSION=2.32.10
SDL_SHA256=83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2

DEPS="$HERE/deps"
BUILD="$HERE/build"
DIST="$HERE/dist"

# Downloads $1 into windows/deps, checks its SHA-256 ($2) and unpacks it.
fetch() {
    local url="$1" sha="$2" file
    file="$DEPS/$(basename "$url")"
    mkdir -p "$DEPS"
    curl -fL -o "$file" "$url"
    echo "$sha  $file" | shasum -a 256 -c - >/dev/null
    tar -xf "$file" -C "$DEPS"
    rm -f "$file"
}

# 1) llvm-mingw: a ready-made Windows compiler for macOS (Intel and Apple Silicon).
LLVM_DIR="$DEPS/llvm-mingw-$LLVM_MINGW_VERSION-ucrt-macos-universal"
if [ ! -d "$LLVM_DIR" ]; then
    fetch "https://github.com/mstorsjo/llvm-mingw/releases/download/$LLVM_MINGW_VERSION/llvm-mingw-$LLVM_MINGW_VERSION-ucrt-macos-universal.tar.xz" \
        "$LLVM_MINGW_SHA256"
    # Downloaded binaries are quarantined by macOS; this one is checksum-verified.
    xattr -dr com.apple.quarantine "$LLVM_DIR" 2>/dev/null || true
fi
export PATH="$LLVM_DIR/bin:$PATH"

# 2) SDL2 development files for MinGW.
SDL_DIR="$DEPS/SDL2-$SDL_VERSION"
if [ ! -d "$SDL_DIR" ]; then
    fetch "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL2-devel-$SDL_VERSION-mingw.tar.gz" \
        "$SDL_SHA256"
fi
SDL_PREFIX="$SDL_DIR/x86_64-w64-mingw32"

# 3) Configure and build.
cmake -S "$ROOT" -B "$BUILD" \
    -DCMAKE_TOOLCHAIN_FILE="$HERE/mingw-toolchain.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DSDL2_DIR="$SDL_PREFIX/lib/cmake/SDL2"
cmake --build "$BUILD" -j 8

# 4) Package: the exe, SDL2.dll beside it, a short read-me and the credits / licenses.
APPDIR="$DIST/HP Tracker"
rm -rf "$DIST"
mkdir -p "$APPDIR"
cp "$BUILD/HP Tracker.exe" "$APPDIR/"
cp "$SDL_PREFIX/bin/SDL2.dll" "$APPDIR/"
cp "$HERE/README.txt" "$APPDIR/README.txt"
cp "$ROOT/legal/CREDITS.txt" "$APPDIR/CREDITS.txt"
(cd "$DIST" && zip -qr "HP-Tracker-windows.zip" "HP Tracker")

echo
echo "Done:"
echo "  windows/dist/HP Tracker/"
echo "  windows/dist/HP-Tracker-windows.zip"
