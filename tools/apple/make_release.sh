#!/usr/bin/env bash
# The Mac and iPhone/iPad files a player downloads, made from a build on a Mac: the counterpart of
# tools/make_release.py for Windows and Linux. It builds nothing and runs nothing; the release
# workflow (.github/workflows/release.yml) builds, calls this, and publishes.
#
# Usage:  bash tools/apple/make_release.sh macos <build dir> <out dir>   -> <out>/Penumbra-macOS.zip
#         bash tools/apple/make_release.sh ios   <build dir> <out dir>   -> <out>/Penumbra-iOS.ipa
#
# macos  Penumbra.app (tools/apple/make_app.sh, signed ad hoc) and "HOW TO PLAY.txt"
#        (game/macos/how-to-play.txt) in a folder Penumbra/, zipped with ditto, Apple's own way to
#        zip a signed bundle: it keeps the executable bit and the bundle's seal, which a plain zip
#        writer loses ("is damaged and can't be opened" on the player's Mac). No AppleDouble files,
#        extended attributes or quarantine flags go in. Then the zip is unpacked again and the
#        signature verified strictly, as a player's Finder would unpack it.
# ios    the device build (iphoneos, arm64) assembled by make_app.sh as Payload/Penumbra.app and
#        zipped: an .ipa with no signature but the ad-hoc one, for a sideloading tool (Sideloadly,
#        AltStore, SideStore) to sign with the player's own Apple ID. It refuses one that links
#        anything outside the system or is encrypted, which no such tool could install.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PLATFORM="${1:-}"
BUILD="${2:-}"
OUT="${3:-}"

if [ "$PLATFORM" != macos ] && [ "$PLATFORM" != ios ] || [ -z "$BUILD" ] || [ -z "$OUT" ]; then
    sed -n '2,8p' "$0" >&2
    exit 2
fi
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

if [ "$PLATFORM" = macos ]; then
    STAGE="$WORK/stage/Penumbra"
    mkdir -p "$STAGE"
    bash "$REPO/tools/apple/make_app.sh" macos "$BUILD" "$STAGE/Penumbra.app"
    # Beside the app, never inside it: anything added to the bundle after make_app.sh signed it
    # breaks the seal. UTF-8 with a BOM, so TextEdit shows the accents whatever it guesses.
    { printf '\xEF\xBB\xBF'; tr -d '\r' < "$REPO/game/macos/how-to-play.txt"; } > "$STAGE/HOW TO PLAY.txt"

    ZIP="$OUT/Penumbra-macOS.zip"
    rm -f "$ZIP"
    ditto -c -k --keepParent --norsrc --noextattr --noqtn --noacl "$STAGE" "$ZIP"

    if unzip -Z1 "$ZIP" | grep -E '(^|/)__MACOSX/|(^|/)\._'; then
        echo "$ZIP holds AppleDouble files" >&2
        exit 1
    fi
    mkdir -p "$WORK/unpacked"
    ditto -x -k "$ZIP" "$WORK/unpacked"
    APP="$WORK/unpacked/Penumbra/Penumbra.app"
    test -x "$APP/Contents/MacOS/Penumbra"
    test -f "$WORK/unpacked/Penumbra/HOW TO PLAY.txt"
    codesign --verify --deep --strict --verbose=2 "$APP"
    echo "architectures: $(lipo -archs "$APP/Contents/MacOS/Penumbra")"
    plutil -p "$APP/Contents/Info.plist" | grep -E 'CFBundle(ShortVersionString|Version|Identifier)|LSMinimumSystemVersion'
    echo "Wrote $ZIP ($(du -h "$ZIP" | cut -f1))."
else
    PAYLOAD="$WORK/ipa/Payload"
    mkdir -p "$PAYLOAD"
    bash "$REPO/tools/apple/make_app.sh" ios "$BUILD" "$PAYLOAD/Penumbra.app" iphoneos
    APP="$PAYLOAD/Penumbra.app"

    echo "architectures: $(lipo -archs "$APP/Penumbra")"
    # Only the system's libraries and frameworks: a sideloading tool signs what is in the bundle,
    # and the program links MoltenVK statically, so nothing else may be named.
    if otool -L "$APP/Penumbra" | tail -n +2 | grep -v -E '^\s+(/System/Library/|/usr/lib/)'; then
        echo "Penumbra links something outside the system" >&2
        exit 1
    fi
    if otool -l "$APP/Penumbra" | grep -A4 LC_ENCRYPTION_INFO | grep -q 'cryptid 1'; then
        echo "Penumbra is encrypted" >&2
        exit 1
    fi
    codesign --verify --strict --verbose=2 "$APP"
    plutil -p "$APP/Info.plist" | grep -E 'CFBundle(ShortVersionString|Version|Identifier)|MinimumOSVersion|DTPlatformName'

    IPA="$OUT/Penumbra-iOS.ipa"
    rm -f "$IPA"
    (cd "$WORK/ipa" && zip -q -r -y -X "$IPA" Payload)
    if [ "$(unzip -Z1 "$IPA" | head -1)" != "Payload/" ]; then
        echo "$IPA does not start with Payload/" >&2
        exit 1
    fi
    if unzip -Z1 "$IPA" | grep -E '(^|/)__MACOSX/|(^|/)\._'; then
        echo "$IPA holds AppleDouble files" >&2
        exit 1
    fi
    echo "Wrote $IPA ($(du -h "$IPA" | cut -f1))."
fi
