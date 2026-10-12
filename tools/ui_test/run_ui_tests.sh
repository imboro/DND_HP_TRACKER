#!/bin/bash
# Builds a test copy of the app that drives itself with simulated input, runs it,
# and prints PASS/FAIL per check. Screenshots go to build/ui_test/*.png.
#
#   tools/ui_test/run_ui_tests.sh
#
# The app source (gui_main.cpp) is not modified: make_harness.py injects the
# test body into a copy under build/ui_test/.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
OUT="$ROOT/build/ui_test"
export PATH="$HOME/homebrew/bin:$PATH"

mkdir -p "$OUT"
cd "$ROOT"
rm -f "$OUT"/gui_test "$OUT"/test_save.txt "$OUT"/test_library.txt "$OUT"/test_settings.txt "$OUT"/test_party.txt "$OUT"/*.bmp "$OUT"/*.png

python3 "$HERE/make_harness.py" gui_main.cpp "$HERE/test_body.inc" "$OUT/gui_test.cpp" "$OUT"

I=third_party/imgui
g++ -std=c++17 -O1 -I$I -I$I/backends -I. -Ithird_party/httplib -Ithird_party/qrcodegen $(sdl2-config --cflags) "$OUT/gui_test.cpp" \
    creature.cpp encounter.cpp difficulty.cpp player_server.cpp third_party/qrcodegen/qrcodegen.cpp \
    $I/imgui.cpp $I/imgui_draw.cpp $I/imgui_tables.cpp $I/imgui_widgets.cpp \
    $I/backends/imgui_impl_sdl2.cpp $I/backends/imgui_impl_sdlrenderer2.cpp \
    $(sdl2-config --libs) -o "$OUT/gui_test" 2> "$OUT/build.log" || { grep -E "error" "$OUT/build.log" | head -20; exit 1; }

# WebKit snapshots of the player screen page (built once).
[ -x "$OUT/phone_snapshot" ] || swiftc -O "$HERE/phone_snapshot.swift" -o "$OUT/phone_snapshot" 2>> "$OUT/build.log"

"$OUT/gui_test"

for f in "$OUT"/*.bmp; do
    [ -e "$f" ] && sips -s format png "$f" --out "${f%.bmp}.png" >/dev/null 2>&1 && rm -f "$f"
done
ls "$OUT"/*.png 2>/dev/null | xargs -n1 basename
