#!/bin/bash
# Builds the Mac and Windows apps and puts them in Downloads/, the folder people
# download from:
#
#   ./make_downloads.sh
#
#   Downloads/README.md                       which one to download (version, date)
#   Downloads/Windows/DnD-HP-Tracker-Windows.zip
#   Downloads/Mac/DnD-HP-Tracker-Mac.zip
#
# The version comes from project(... VERSION x.y ...) in CMakeLists.txt.
# Commit and push the Downloads folder afterwards.
set -euo pipefail
cd "$(dirname "$0")"

VERSION=$(sed -nE 's/^project\(DnDHpTracker VERSION ([0-9.]+).*/\1/p' CMakeLists.txt)
DATE=$(date +"%-d %B %Y")
[ -n "$VERSION" ] || { echo "No version found in CMakeLists.txt"; exit 1; }

./package_mac.sh
./windows/build_windows.sh

mkdir -p Downloads/Windows Downloads/Mac
rm -f Downloads/Windows/*.zip Downloads/Mac/*.zip
cp windows/dist/DnD-HP-Tracker-windows.zip Downloads/Windows/DnD-HP-Tracker-Windows.zip
cp dist/DnD-HP-Tracker-mac.zip Downloads/Mac/DnD-HP-Tracker-Mac.zip

WIN_MB=$(du -k Downloads/Windows/DnD-HP-Tracker-Windows.zip | awk '{printf "%.1f", $1 / 1024}')
MAC_MB=$(du -k Downloads/Mac/DnD-HP-Tracker-Mac.zip | awk '{printf "%.1f", $1 / 1024}')

cat > Downloads/README.md <<EOF
# Download DnD HP Tracker

**Version $VERSION** · $DATE

Pick your computer. Each link downloads a zip; the page next to it explains how to
open the app the first time.

| Your computer | Download | How to open it |
|---|---|---|
| **Windows** 10 or 11 | [⬇ DnD-HP-Tracker-Windows.zip](https://github.com/imboro/DND_HP_TRACKER/raw/main/Downloads/Windows/DnD-HP-Tracker-Windows.zip) ($WIN_MB MB) | [Windows guide](Windows/) |
| **Mac** with Apple Silicon (M1 or newer) | [⬇ DnD-HP-Tracker-Mac.zip](https://github.com/imboro/DND_HP_TRACKER/raw/main/Downloads/Mac/DnD-HP-Tracker-Mac.zip) ($MAC_MB MB) | [Mac guide](Mac/) |

Nothing to install: unzip and open. The app isn't signed by a paid developer
account, so the first time Windows or macOS asks you to confirm; the guides show how.

[← Back to the project](../)
EOF

echo
echo "Downloads ready (version $VERSION):"
ls -la Downloads/Windows/*.zip Downloads/Mac/*.zip
