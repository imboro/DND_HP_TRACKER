#!/bin/bash
# Publishes HP Tracker to the public repo imboro/hp-tracker (the code stays private):
#
#   ./publish_release.sh            build both apps, release them, update the website
#   ./publish_release.sh --site     only update the website
#
# 1. Builds the Mac and Windows zips (make_downloads.sh).
# 2. Creates the GitHub release v<version> on imboro/hp-tracker with both zips
#    (or replaces its zips if that release exists). The website's download buttons
#    point at "the latest release", so they follow automatically.
# 3. Copies website/ (plus CREDITS.txt) into the public repo and pushes it; GitHub
#    Pages serves it at https://imboro.github.io/hp-tracker/.
#
# Needs: gh, logged in (gh auth login). The version comes from CMakeLists.txt.
set -euo pipefail
cd "$(dirname "$0")"
export PATH="$HOME/homebrew/bin:$PATH"

PUBLIC=imboro/hp-tracker
SITE_DIR=build/public-site
VERSION=$(sed -nE 's/^project\(HpTracker VERSION ([0-9.]+).*/\1/p' CMakeLists.txt)
[ -n "$VERSION" ] || { echo "No version found in CMakeLists.txt"; exit 1; }
gh auth status >/dev/null 2>&1 || { echo "Log in first: gh auth login"; exit 1; }

if [ "${1:-}" != "--site" ]; then
    ./make_downloads.sh
    WIN=Downloads/Windows/HP-Tracker-Windows.zip
    MAC=Downloads/Mac/HP-Tracker-Mac.zip
    if gh release view "v$VERSION" --repo "$PUBLIC" >/dev/null 2>&1; then
        gh release upload "v$VERSION" "$WIN" "$MAC" --repo "$PUBLIC" --clobber
    else
        gh release create "v$VERSION" "$WIN" "$MAC" --repo "$PUBLIC" --title "HP Tracker $VERSION" \
            --notes "Download for Windows 10/11 or Mac (Apple Silicon). How to open it the first time: https://imboro.github.io/hp-tracker/#download"
    fi
fi

# The website: a fresh copy of website/ in the public repo's checkout.
if [ ! -d "$SITE_DIR/.git" ]; then
    rm -rf "$SITE_DIR"
    gh repo clone "$PUBLIC" "$SITE_DIR"
fi
git -C "$SITE_DIR" pull --quiet --ff-only || true
find "$SITE_DIR" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
cp -R website/. "$SITE_DIR/"
mv "$SITE_DIR/repo-README.md" "$SITE_DIR/README.md"
cp legal/CREDITS.txt "$SITE_DIR/CREDITS.txt"
touch "$SITE_DIR/.nojekyll"  # serve the files as they are
# The version the page shows before it asks GitHub for the latest release.
sed -i '' -E "s/(<span class=\"js-version\">)[0-9.]+(<\/span>)/\1$VERSION\2/g" "$SITE_DIR/index.html"

git -C "$SITE_DIR" add -A
if git -C "$SITE_DIR" diff --cached --quiet; then
    echo "Website unchanged."
else
    git -C "$SITE_DIR" commit --quiet -m "Website for HP Tracker $VERSION"
    git -C "$SITE_DIR" push --quiet origin HEAD
    echo "Website pushed."
fi
echo "Done: https://imboro.github.io/hp-tracker/"
