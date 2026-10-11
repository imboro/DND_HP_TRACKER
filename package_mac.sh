#!/bin/bash
# Builds a self-contained "DnD HP Tracker.app" (SDL libraries copied inside)
# and zips it, so it runs on Macs that don't have Homebrew or SDL installed.
set -euo pipefail
cd "$(dirname "$0")"

# Use the Homebrew-in-home-directory install if present.
export PATH="$HOME/homebrew/bin:$PATH"

APP="dist/DnD HP Tracker.app"
EXE="$APP/Contents/MacOS/DnD HP Tracker"
FW="$APP/Contents/Frameworks"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
rm -rf dist
cmake --install build --prefix dist
mkdir -p "$FW"

# Copy every non-system dylib the app needs into the bundle and point at it.
bundle_deps() {
    local file="$1" dep name
    while read -r dep; do
        case "$dep" in
            /usr/lib/*|/System/*|@*) continue ;;
        esac
        name=$(basename "$dep")
        if [ ! -f "$FW/$name" ]; then
            cp "$dep" "$FW/$name"
            chmod u+w "$FW/$name"
            install_name_tool -id "@rpath/$name" "$FW/$name"
            bundle_deps "$FW/$name"
        fi
        install_name_tool -change "$dep" "@rpath/$name" "$file"
    done < <(otool -L "$file" | tail -n +2 | awk '{print $1}')
}

install_name_tool -add_rpath "@executable_path/../Frameworks" "$EXE"
bundle_deps "$EXE"

# Homebrew's SDL2 is sdl2-compat, which loads SDL3 at runtime (dlopen) instead
# of linking it, so otool can't see it. It dlopen()s "@loader_path/libSDL3.dylib"
# (no ".0"), so bundle it next to libSDL2 under exactly that name.
SDL3_LIB="$(brew --prefix sdl3 2>/dev/null)/lib/libSDL3.0.dylib"
if [ -f "$SDL3_LIB" ]; then
    cp "$SDL3_LIB" "$FW/libSDL3.dylib"
    chmod u+w "$FW/libSDL3.dylib"
    install_name_tool -id "@rpath/libSDL3.dylib" "$FW/libSDL3.dylib"
    bundle_deps "$FW/libSDL3.dylib"
else
    echo "warning: libSDL3 not found; the app may not start on other Macs" >&2
fi

# Changing install names invalidates signatures; Apple Silicon needs one.
codesign --force --deep --sign - "$APP"

ditto -c -k --keepParent "$APP" "dist/DnD-HP-Tracker-mac.zip"
echo
echo "Done:"
echo "  $APP"
echo "  dist/DnD-HP-Tracker-mac.zip"
