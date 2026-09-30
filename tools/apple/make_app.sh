#!/usr/bin/env bash
# Assemble Penumbra.app from a build, on a Mac: the counterpart of tools/package.bat. It builds
# nothing and runs nothing.
#
# Usage:  bash tools/apple/make_app.sh macos <build dir> <out/Penumbra.app>
#         bash tools/apple/make_app.sh ios   <build dir> <out/Penumbra.app> [iphonesimulator|iphoneos]
#
# What goes in, and why each piece is where it is:
#   the executable     <build>/game/Penumbra (or the one inside <build>/game/Penumbra.app, if the
#                      generator made a bundle): Contents/MacOS/ on a Mac, the bundle's root on iOS.
#   Info.plist         game/macos/Info.plist or game/ios/Info.plist, with the minimum OS version the
#                      executable was built for, and on iOS the platform (simulator or device).
#   original/          extracted/app, the original's DATA only (package.bat's list: no .exe, .dll,
#                      .as, .cg or editor project), read in place by the game, never written.
#   data/              game/data: strings.json, the English images, the touch controls.
#   engine/assets/shaders/*.spv
#                      the engine's compiled shaders. Not assets/shaders, which would make the
#                      bundle itself the engine's asset root (ChooseAssetRoot): the engine writes
#                      where it runs, and a bundle is read-only. The game's entry copies them to a
#                      folder it may write in and runs from there (game/macos/MacMain.mm,
#                      game/ios/IOSMain.mm).
#   LICENSE.txt, licenses/
#                      the licence texts, as tools/package.bat puts them beside Penumbra.exe.
#   the icon           the original's penumbra.ico (48 px at most), scaled up: Penumbra.icns on a
#                      Mac, AppIcon*.png on iOS.
# In Contents/Resources on a Mac; at the root of the bundle on iOS, which is flat.
#
# Signed ad hoc (codesign -s -), which is no identity: what arm64 needs to run it at all, and what
# a simulator installs. A device or the App Store needs a real signature, and a Mac run from a
# download needs notarising; neither is done here.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PLATFORM="${1:-}"
BUILD="${2:-}"
APP="${3:-}"
SDK="${4:-iphonesimulator}"

if [ "$PLATFORM" != macos ] && [ "$PLATFORM" != ios ] || [ -z "$BUILD" ] || [ -z "$APP" ]; then
    sed -n '2,8p' "$0" >&2
    exit 2
fi

EXE="$BUILD/game/Penumbra"
if [ -d "$BUILD/game/Penumbra.app" ]; then
    if [ -f "$BUILD/game/Penumbra.app/Contents/MacOS/Penumbra" ]; then
        EXE="$BUILD/game/Penumbra.app/Contents/MacOS/Penumbra"
    else
        EXE="$BUILD/game/Penumbra.app/Penumbra"
    fi
fi
if [ ! -f "$EXE" ]; then
    echo "No executable at $EXE: build the Penumbra target first." >&2
    exit 1
fi
for need in "$REPO/extracted/app/data.enml" "$REPO/game/data/strings.json" "$REPO/engine/assets/shaders/frag.spv"; do
    if [ ! -f "$need" ]; then
        echo "Missing $need." >&2
        exit 1
    fi
done

rm -rf "$APP"
if [ "$PLATFORM" = macos ]; then
    BIN="$APP/Contents/MacOS"
    RES="$APP/Contents/Resources"
    PLIST="$APP/Contents/Info.plist"
else
    BIN="$APP"
    RES="$APP"
    PLIST="$APP/Info.plist"
fi
mkdir -p "$BIN" "$RES"

cp "$EXE" "$BIN/Penumbra"
cp "$REPO/game/$PLATFORM/Info.plist" "$PLIST"

# The minimum OS the executable was linked for (LC_BUILD_VERSION), so the plist cannot promise more.
MINOS="$(otool -l "$BIN/Penumbra" | awk '/LC_BUILD_VERSION/ { found = 1 } found && $1 == "minos" { print $2; exit }')"
if [ -n "$MINOS" ]; then
    if [ "$PLATFORM" = macos ]; then
        plutil -replace LSMinimumSystemVersion -string "$MINOS" "$PLIST"
    else
        plutil -replace MinimumOSVersion -string "$MINOS" "$PLIST"
    fi
fi
if [ "$PLATFORM" = ios ]; then
    if [ "$SDK" = iphonesimulator ]; then
        plutil -replace CFBundleSupportedPlatforms -json '["iPhoneSimulator"]' "$PLIST"
    else
        plutil -replace CFBundleSupportedPlatforms -json '["iPhoneOS"]' "$PLIST"
    fi
    # The build facts Xcode records in every app it builds, which a bundle made without Xcode
    # lacks; installers and sideloading tools may look for them.
    SDK_VERSION="$(xcrun --sdk "$SDK" --show-sdk-version)"
    plutil -replace DTPlatformName -string "$SDK" "$PLIST"
    plutil -replace DTPlatformVersion -string "$SDK_VERSION" "$PLIST"
    plutil -replace DTSDKName -string "$SDK$SDK_VERSION" "$PLIST"
    plutil -replace DTSDKBuild -string "$(xcrun --sdk "$SDK" --show-sdk-build-version)" "$PLIST"
fi
plutil -lint "$PLIST" >/dev/null

rsync -a --exclude '*.exe' --exclude '*.dll' --exclude '*.as' --exclude '*.cg' --exclude '*.ethproj' \
    --exclude 'readme.txt' "$REPO/extracted/app/" "$RES/original/"
rsync -a "$REPO/game/data/" "$RES/data/"
mkdir -p "$RES/engine/assets/shaders"
cp "$REPO"/engine/assets/shaders/*.spv "$RES/engine/assets/shaders/"

# The licences, as tools/package.bat puts them beside Penumbra.exe: the program carries LGPL-3.0
# code (game/script, game/eth) and the engine's third-party code, whose notices travel with it.
mkdir -p "$RES/licenses"
cp "$REPO/LICENSE" "$RES/LICENSE.txt"
cp "$REPO/engine/LICENSE" "$RES/licenses/Supersonic-Engine-LICENSE.txt"
cp "$REPO/engine/THIRD_PARTY_LICENSES.md" "$RES/licenses/Supersonic-Engine-THIRD_PARTY_LICENSES.md"
cp "$REPO/LICENSE.md" "$RES/licenses/LICENSE.md"
cp "$REPO/licenses/LGPL-3.0.txt" "$RES/licenses/LGPL-3.0.txt"
cp "$REPO/licenses/GPL-3.0.txt" "$RES/licenses/GPL-3.0.txt"

# The icon, from the original's largest image. Not fatal: an app without an icon still runs.
ICONWORK="$(mktemp -d)"
if python3 "$REPO/tools/apple/ico_to_png.py" "$REPO/extracted/app/penumbra.ico" "$ICONWORK/icon.png"; then
    if [ "$PLATFORM" = macos ]; then
        SET="$ICONWORK/Penumbra.iconset"
        mkdir -p "$SET"
        for size in 16 32 128 256 512; do
            sips -z "$size" "$size" "$ICONWORK/icon.png" --out "$SET/icon_${size}x${size}.png" >/dev/null
            double=$((size * 2))
            sips -z "$double" "$double" "$ICONWORK/icon.png" --out "$SET/icon_${size}x${size}@2x.png" >/dev/null
        done
        iconutil -c icns "$SET" -o "$RES/Penumbra.icns" || echo "warning: iconutil failed; no icon" >&2
    else
        # CFBundleIconFiles in game/ios/Info.plist: 60 pt (iPhone), 76 and 83.5 pt (iPad).
        sips -z 120 120 "$ICONWORK/icon.png" --out "$RES/AppIcon60x60@2x.png" >/dev/null
        sips -z 180 180 "$ICONWORK/icon.png" --out "$RES/AppIcon60x60@3x.png" >/dev/null
        sips -z 152 152 "$ICONWORK/icon.png" --out "$RES/AppIcon76x76@2x~ipad.png" >/dev/null
        sips -z 167 167 "$ICONWORK/icon.png" --out "$RES/AppIcon83.5x83.5@2x~ipad.png" >/dev/null
    fi
else
    echo "warning: the icon could not be converted; the app has none" >&2
fi
rm -rf "$ICONWORK"

codesign --force --sign - --timestamp=none "$APP"
echo "Assembled $APP ($(du -sh "$APP" | cut -f1)), minimum OS ${MINOS:-unknown}."
