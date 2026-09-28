#!/usr/bin/env bash
# Build Penumbra for Android and package a debug APK, without Gradle.
#
#   tools/build_android.sh [options]            (Git Bash, from anywhere)
#
#   --abi x86_64|arm64-v8a|all   which ABIs to build (default x86_64, the emulator's;
#                                 all = both in one APK)
#   --config Release|Debug|RelWithDebInfo       native build type (default Release: the
#                                 APK is a debug APK - debuggable, debug-signed - either way)
#   --install                    adb install -r the APK (on --serial)
#   --serial <serial>            the device for --install/--run (default emulator-5560)
#   --run "<flags>"              after installing, leave <flags> for the next launch in
#                                 penumbra_args.txt (game/android/AndroidMain.cpp) and start
#                                 the app; --run "" starts it with none. A --screenshot goes
#                                 to /data/user/0/com.ivancvetanovic.penumbra/files/<name>.png;
#                                 read it back with
#                                 adb exec-out run-as com.ivancvetanovic.penumbra cat files/<name>.png
#   --package-only               skip configure and build; repackage what is built
#
# Uses the SDK at $ANDROID_SDK (default %LOCALAPPDATA%/Android/Sdk): NDK 28.2.13676358's
# toolchain file, its cmake 3.22.1 and ninja, build-tools 35.0.0 (aapt2, zipalign,
# apksigner) and platform 35's android.jar; a JDK 17 for keytool and apksigner; Python 3
# with Pillow (tools/android_package.py). Build trees: build-android-<abi>/ at the repo
# root. Output: out/android/Penumbra-debug.apk.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK="${ANDROID_SDK:-$(cygpath -u "${LOCALAPPDATA:-C:/Users/$USERNAME/AppData/Local}")/Android/Sdk}"
NDK="$SDK/ndk/28.2.13676358"
CMAKE="$SDK/cmake/3.22.1/bin/cmake.exe"
NINJA="$SDK/cmake/3.22.1/bin/ninja.exe"
BUILD_TOOLS="$SDK/build-tools/35.0.0"
ANDROID_JAR="$SDK/platforms/android-35/android.jar"
ADB="$SDK/platform-tools/adb.exe"
STRIP="$NDK/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe"
PACKAGE="com.ivancvetanovic.penumbra"
MIN_SDK=26
TARGET_SDK=35

ABI_ARG="x86_64"
CONFIG="Release"
INSTALL=0
SERIAL="emulator-5560"
RUN=0
RUN_FLAGS=""
PACKAGE_ONLY=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --abi) ABI_ARG="$2"; shift 2 ;;
        --config) CONFIG="$2"; shift 2 ;;
        --install) INSTALL=1; shift ;;
        --serial) SERIAL="$2"; shift 2 ;;
        --run) RUN=1; INSTALL=1; RUN_FLAGS="$2"; shift 2 ;;
        --package-only) PACKAGE_ONLY=1; shift ;;
        -h|--help) awk 'NR > 1 && /^set -euo/ { exit } NR > 1' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
done
case "$ABI_ARG" in
    all) ABIS=(x86_64 arm64-v8a) ;;
    x86_64|arm64-v8a) ABIS=("$ABI_ARG") ;;
    *) echo "--abi wants x86_64, arm64-v8a or all" >&2; exit 2 ;;
esac

for tool in "$NDK/build/cmake/android.toolchain.cmake" "$CMAKE" "$NINJA" "$BUILD_TOOLS/aapt2.exe" "$ANDROID_JAR"; do
    [[ -e "$tool" ]] || { echo "missing: $tool" >&2; exit 1; }
done
win() { cygpath -m "$1"; }

# The JDK: JAVA_HOME if it has keytool, else whatever keytool is on PATH.
if [[ -n "${JAVA_HOME:-}" && -x "$(cygpath -u "$JAVA_HOME")/bin/keytool.exe" ]]; then
    JAVA_BIN="$(cygpath -u "$JAVA_HOME")/bin"
else
    JAVA_BIN="$(dirname "$(command -v keytool)")"
fi
export JAVA_HOME="$(cygpath -m "$(dirname "$JAVA_BIN")")"
export PATH="$JAVA_BIN:$PATH"

OUT="$REPO/out/android"
mkdir -p "$OUT"

# ---- Configure and build, one tree per ABI ------------------------------------
LIB_SPECS=()
for abi in "${ABIS[@]}"; do
    tree="$REPO/build-android-$abi"
    if [[ $PACKAGE_ONLY -eq 0 ]]; then
        if [[ ! -f "$tree/CMakeCache.txt" ]]; then
            "$CMAKE" -S "$(win "$REPO")" -B "$(win "$tree")" -G Ninja \
                -DCMAKE_MAKE_PROGRAM="$(win "$NINJA")" \
                -DCMAKE_TOOLCHAIN_FILE="$(win "$NDK/build/cmake/android.toolchain.cmake")" \
                -DANDROID_ABI="$abi" \
                -DANDROID_PLATFORM="android-$MIN_SDK" \
                -DANDROID_STL=c++_static \
                -DCMAKE_BUILD_TYPE="$CONFIG" \
                -DSUPERSONIC_ENABLE_VALIDATION=OFF \
                -DPENUMBRA_BUILD_TESTS=OFF
        fi
        # At most six jobs: another session compiles on this machine too.
        "$CMAKE" --build "$(win "$tree")" --target Penumbra -j 6
    fi
    lib="$tree/game/libPenumbra.so"
    [[ -f "$lib" ]] || { echo "not built: $lib" >&2; exit 1; }
    mkdir -p "$OUT/lib/$abi"
    "$STRIP" --strip-unneeded -o "$(win "$OUT/lib/$abi/libPenumbra.so")" "$(win "$lib")"
    LIB_SPECS+=("$abi=$(win "$OUT/lib/$abi/libPenumbra.so")")
done

# ---- Package ------------------------------------------------------------------
STAGE="$OUT/stage"
python "$(win "$REPO/tools/android_package.py")" stage "$(win "$STAGE")"

rm -f "$OUT/res.zip"
"$BUILD_TOOLS/aapt2.exe" compile --dir "$(win "$STAGE/res")" -o "$(win "$OUT/res.zip")"
"$BUILD_TOOLS/aapt2.exe" link \
    -o "$(win "$OUT/unsigned.apk")" \
    -I "$(win "$ANDROID_JAR")" \
    --manifest "$(win "$REPO/game/android/AndroidManifest.xml")" \
    -A "$(win "$STAGE/assets")" \
    -R "$(win "$OUT/res.zip")" \
    --min-sdk-version "$MIN_SDK" --target-sdk-version "$TARGET_SDK" \
    --debug-mode --auto-add-overlay
python "$(win "$REPO/tools/android_package.py")" addlibs "$(win "$OUT/unsigned.apk")" "$(win "$OUT/withlibs.apk")" "${LIB_SPECS[@]}"
"$BUILD_TOOLS/zipalign.exe" -f -P 16 4 "$(win "$OUT/withlibs.apk")" "$(win "$OUT/aligned.apk")"

# The machine's debug keystore when there is one (what Android Studio signs
# with), otherwise one made here once.
KEYSTORE="$HOME/.android/debug.keystore"
if [[ ! -f "$KEYSTORE" ]]; then
    KEYSTORE="$OUT/debug.keystore"
    if [[ ! -f "$KEYSTORE" ]]; then
        keytool -genkeypair -keystore "$(win "$KEYSTORE")" -storepass android -keypass android \
            -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 \
            -dname "CN=Android Debug,O=Android,C=US"
    fi
fi
APK="$OUT/Penumbra-debug.apk"
cmd //c "$(win "$BUILD_TOOLS/apksigner.bat")" sign --ks "$(win "$KEYSTORE")" --ks-pass pass:android \
    --key-pass pass:android --ks-key-alias androiddebugkey --out "$(win "$APK")" "$(win "$OUT/aligned.apk")"
rm -f "$OUT/unsigned.apk" "$OUT/withlibs.apk" "$OUT/aligned.apk"
echo "APK: $APK ($(( $(stat -c %s "$APK") / 1024 / 1024 )) MB)"

# ---- Install and run ----------------------------------------------------------
# Device paths start with /, which Git Bash would otherwise rewrite into
# C:/Program Files/Git/... on the way to adb. Local paths here go through win().
export MSYS_NO_PATHCONV=1
if [[ $INSTALL -eq 1 ]]; then
    "$ADB" -s "$SERIAL" install -r "$(win "$APK")"
fi
if [[ $RUN -eq 1 ]]; then
    # install -r has already stopped the app, and a force-stop while it is
    # presenting has taken the emulator down with it (swiftshader_indirect).
    if [[ -n "$RUN_FLAGS" ]]; then
        # Into the app's private files directory, through run-as (the APK is
        # debuggable): from Android 11 the shell cannot create files in the
        # app's external directory.
        printf '%s\n' "$RUN_FLAGS" | "$ADB" -s "$SERIAL" exec-in run-as "$PACKAGE" sh -c 'cat > files/penumbra_args.txt'
    fi
    "$ADB" -s "$SERIAL" shell am start -n "$PACKAGE/android.app.NativeActivity"
fi
