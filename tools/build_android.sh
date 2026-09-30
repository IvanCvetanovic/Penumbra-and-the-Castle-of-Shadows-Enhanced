#!/usr/bin/env bash
# Build Penumbra for Android and package an APK, without Gradle: a debug APK by
# default, the release APK with --release.
#
#   tools/build_android.sh [options]            (Git Bash, from anywhere)
#
#   --abi x86_64|arm64-v8a|all   which ABIs to build (default x86_64, the emulator's;
#                                 all = both in one APK; --release defaults to all)
#   --config Release|Debug|RelWithDebInfo       native build type (default Release: without
#                                 --release the APK is a debug APK - debuggable, debug-signed -
#                                 either way)
#   --release                    THE RELEASE APK: not debuggable (aapt2 without --debug-mode),
#                                 signed with the release key, never the debug one, and checked
#                                 after signing (apksigner verify, the certificate against the
#                                 keystore's, no application-debuggable). Output:
#                                 out/release/Penumbra-Android.apk. Needs --config Release and
#                                 build trees configured Release without validation.
#                                 The key, each overridable by a flag or the environment:
#     --keystore <file>             $PENUMBRA_KEYSTORE, else ~/.android/penumbra-release.jks
#     --keystore-pass-file <file>   $PENUMBRA_KEYSTORE_PASS_FILE, else
#                                   ~/.android/penumbra-release-password.txt: the password,
#                                   its first line (keystore and key share it, PKCS12)
#     --key-alias <alias>           $PENUMBRA_KEY_ALIAS, else penumbra
#                                 The key lives outside the repository and is never copied in.
#                                 A release APK cannot be installed over a debug one (another
#                                 signer): adb uninstall com.ivancvetanovic.penumbra first.
#   --install                    adb install -r the APK (on --serial)
#   --serial <serial>            the device for --install/--run (default emulator-5560)
#   --run "<flags>"              after installing, leave <flags> for the next launch in
#                                 penumbra_args.txt (game/android/AndroidMain.cpp) and start
#                                 the app; --run "" starts it with none. A --screenshot goes
#                                 to /data/user/0/com.ivancvetanovic.penumbra/files/<name>.png;
#                                 read it back with
#                                 adb exec-out run-as com.ivancvetanovic.penumbra cat files/<name>.png
#                                 (a debug APK only: a release APK has no run-as, so with
#                                 --release only --run "" is taken)
#   --package-only               skip configure and build; repackage what is built
#
# Uses the SDK at $ANDROID_SDK, else $ANDROID_HOME, else $ANDROID_SDK_ROOT, else Android Studio's
# default %LOCALAPPDATA%/Android/Sdk (Windows paths are taken either way): NDK 28.2.13676358's
# toolchain file, its cmake 3.22.1 and ninja, build-tools 35.0.0 (aapt2, d8, zipalign,
# apksigner) and platform 35's android.jar; a JDK 17 for javac, keytool and apksigner; Python 3
# with Pillow (tools/android_package.py). Build trees: build-android-<abi>/ at the repo
# root. Output: out/android/Penumbra-debug.apk, or out/release/Penumbra-Android.apk.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK="${ANDROID_SDK:-${ANDROID_HOME:-${ANDROID_SDK_ROOT:-${LOCALAPPDATA:-C:/Users/$USERNAME/AppData/Local}/Android/Sdk}}}"
SDK="$(cygpath -u "$SDK")"
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

ABI_ARG=""
CONFIG="Release"
INSTALL=0
SERIAL="emulator-5560"
RUN=0
RUN_FLAGS=""
PACKAGE_ONLY=0
RELEASE=0
KEYSTORE_FLAG=""
PASS_FILE_FLAG=""
KEY_ALIAS_FLAG=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --abi) ABI_ARG="$2"; shift 2 ;;
        --config) CONFIG="$2"; shift 2 ;;
        --install) INSTALL=1; shift ;;
        --serial) SERIAL="$2"; shift 2 ;;
        --run) RUN=1; INSTALL=1; RUN_FLAGS="$2"; shift 2 ;;
        --package-only) PACKAGE_ONLY=1; shift ;;
        --release) RELEASE=1; shift ;;
        --keystore) KEYSTORE_FLAG="$2"; shift 2 ;;
        --keystore-pass-file) PASS_FILE_FLAG="$2"; shift 2 ;;
        --key-alias) KEY_ALIAS_FLAG="$2"; shift 2 ;;
        -h|--help) awk 'NR > 1 && /^set -euo/ { exit } NR > 1' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
done
if [[ -z "$ABI_ARG" ]]; then
    if [[ $RELEASE -eq 1 ]]; then ABI_ARG="all"; else ABI_ARG="x86_64"; fi
fi
case "$ABI_ARG" in
    all) ABIS=(x86_64 arm64-v8a) ;;
    x86_64|arm64-v8a) ABIS=("$ABI_ARG") ;;
    *) echo "--abi wants x86_64, arm64-v8a or all" >&2; exit 2 ;;
esac

# The release key: flag, else environment, else the files beside the debug
# keystore in the user's .android folder. Checked before anything is built, and
# there is no fallback: a release signed with the debug key could never be
# updated by the real one.
if [[ $RELEASE -eq 1 ]]; then
    if [[ "$CONFIG" != "Release" ]]; then
        echo "--release builds the native code as Release only (got --config $CONFIG)" >&2; exit 2
    fi
    if [[ $RUN -eq 1 && -n "$RUN_FLAGS" ]]; then
        echo "--release: a release APK is not debuggable, so no run-as to leave flags with; use --run \"\"" >&2; exit 2
    fi
    if [[ -n "${USERPROFILE:-}" ]]; then USER_HOME="$(cygpath -u "$USERPROFILE")"; else USER_HOME="$HOME"; fi
    RELEASE_KEYSTORE="${KEYSTORE_FLAG:-${PENUMBRA_KEYSTORE:-$USER_HOME/.android/penumbra-release.jks}}"
    RELEASE_PASS_FILE="${PASS_FILE_FLAG:-${PENUMBRA_KEYSTORE_PASS_FILE:-$USER_HOME/.android/penumbra-release-password.txt}}"
    RELEASE_ALIAS="${KEY_ALIAS_FLAG:-${PENUMBRA_KEY_ALIAS:-penumbra}}"
    RELEASE_KEYSTORE="$(cygpath -u "$RELEASE_KEYSTORE")"
    RELEASE_PASS_FILE="$(cygpath -u "$RELEASE_PASS_FILE")"
    if [[ ! -f "$RELEASE_KEYSTORE" ]]; then
        echo "--release: no release keystore at $RELEASE_KEYSTORE." >&2
        echo "  Name it with --keystore <file> or PENUMBRA_KEYSTORE. The debug key is never used for a release." >&2
        exit 1
    fi
    if [[ ! -s "$RELEASE_PASS_FILE" ]]; then
        echo "--release: no password file at $RELEASE_PASS_FILE (or it is empty)." >&2
        echo "  Name it with --keystore-pass-file <file> or PENUMBRA_KEYSTORE_PASS_FILE." >&2
        exit 1
    fi
fi

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
    # A tree is configured once and never again here, so a release checks what
    # an older run configured it as: Release, without validation layers.
    if [[ $RELEASE -eq 1 && -f "$tree/CMakeCache.txt" ]]; then
        if ! grep -q '^CMAKE_BUILD_TYPE:STRING=Release$' "$tree/CMakeCache.txt" \
            || ! grep -q '^SUPERSONIC_ENABLE_VALIDATION:[A-Z]*=OFF$' "$tree/CMakeCache.txt"; then
            echo "--release: $tree is not configured Release without validation; delete it and run again" >&2
            exit 1
        fi
    fi
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
        # At most six jobs: another project compiles on this machine too.
        "$CMAKE" --build "$(win "$tree")" --target Penumbra -j 6
    fi
    lib="$tree/game/libPenumbra.so"
    [[ -f "$lib" ]] || { echo "not built: $lib" >&2; exit 1; }
    mkdir -p "$OUT/lib/$abi"
    "$STRIP" --strip-unneeded -o "$(win "$OUT/lib/$abi/libPenumbra.so")" "$(win "$lib")"
    LIB_SPECS+=("$abi=$(win "$OUT/lib/$abi/libPenumbra.so")")
done

# ---- The Java side ---------------------------------------------------------------
# The engine's SupersonicActivity (engine/src/platform/android/java/), the one
# class the manifest names: javac against platform 35's android.jar, then d8
# into classes.dex. Java 8 bytecode, which d8 takes whatever JDK compiled it.
JAVA_SRC="$REPO/engine/src/platform/android/java"
rm -rf "$OUT/classes" "$OUT/dex"
mkdir -p "$OUT/classes" "$OUT/dex"
mapfile -t JAVA_FILES < <(find "$JAVA_SRC" -name '*.java')
JAVA_WIN=()
for f in "${JAVA_FILES[@]}"; do JAVA_WIN+=("$(win "$f")"); done
javac -source 8 -target 8 -Xlint:-options -Xlint:deprecation -encoding UTF-8 \
    -bootclasspath "$(win "$ANDROID_JAR")" -d "$(win "$OUT/classes")" "${JAVA_WIN[@]}"
mapfile -t CLASS_FILES < <(find "$OUT/classes" -name '*.class')
CLASS_WIN=()
for f in "${CLASS_FILES[@]}"; do CLASS_WIN+=("$(win "$f")"); done
cmd //c "$(win "$BUILD_TOOLS/d8.bat")" --release --min-api "$MIN_SDK" --lib "$(win "$ANDROID_JAR")" \
    --output "$(win "$OUT/dex")" "${CLASS_WIN[@]}"
[[ -f "$OUT/dex/classes.dex" ]] || { echo "d8 made no classes.dex" >&2; exit 1; }
LIB_SPECS+=("classes.dex=$(win "$OUT/dex/classes.dex")")

# ---- Package ------------------------------------------------------------------
STAGE="$OUT/stage"
python "$(win "$REPO/tools/android_package.py")" stage "$(win "$STAGE")"

rm -f "$OUT/res.zip"
"$BUILD_TOOLS/aapt2.exe" compile --dir "$(win "$STAGE/res")" -o "$(win "$OUT/res.zip")"
# --debug-mode is what makes an APK debuggable (android:debuggable="true", so
# run-as works); a release links without it.
LINK_MODE=(--debug-mode)
if [[ $RELEASE -eq 1 ]]; then LINK_MODE=(); fi
"$BUILD_TOOLS/aapt2.exe" link \
    -o "$(win "$OUT/unsigned.apk")" \
    -I "$(win "$ANDROID_JAR")" \
    --manifest "$(win "$REPO/game/android/AndroidManifest.xml")" \
    -A "$(win "$STAGE/assets")" \
    -R "$(win "$OUT/res.zip")" \
    --min-sdk-version "$MIN_SDK" --target-sdk-version "$TARGET_SDK" \
    "${LINK_MODE[@]}" --auto-add-overlay
python "$(win "$REPO/tools/android_package.py")" addlibs "$(win "$OUT/unsigned.apk")" "$(win "$OUT/withlibs.apk")" "${LIB_SPECS[@]}"
"$BUILD_TOOLS/zipalign.exe" -f -P 16 4 "$(win "$OUT/withlibs.apk")" "$(win "$OUT/aligned.apk")"

APKSIGNER="$(win "$BUILD_TOOLS/apksigner.bat")"
if [[ $RELEASE -eq 1 ]]; then
    # The release key, its password read from its file by apksigner itself:
    # never on this command line, never echoed. No --key-pass: a PKCS12 key's
    # password is the keystore's, which apksigner tries first, and naming the
    # same file twice makes it read the key's from the file's SECOND line. No
    # v4 signature: that is an .idsig file beside the APK, for adb's
    # incremental installs only, and a player downloads the one file.
    mkdir -p "$REPO/out/release"
    APK="$REPO/out/release/Penumbra-Android.apk"
    rm -f "$APK" "$APK.idsig"
    cmd //c "$APKSIGNER" sign --ks "$(win "$RELEASE_KEYSTORE")" --ks-key-alias "$RELEASE_ALIAS" \
        --ks-pass "file:$(win "$RELEASE_PASS_FILE")" \
        --v4-signing-enabled false --out "$(win "$APK")" "$(win "$OUT/aligned.apk")"
else
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
    cmd //c "$APKSIGNER" sign --ks "$(win "$KEYSTORE")" --ks-pass pass:android \
        --key-pass pass:android --ks-key-alias androiddebugkey --out "$(win "$APK")" "$(win "$OUT/aligned.apk")"
fi
rm -f "$OUT/unsigned.apk" "$OUT/withlibs.apk" "$OUT/aligned.apk"
echo "APK: $APK ($(( $(stat -c %s "$APK") / 1024 / 1024 )) MB)"

# ---- A release checked before anyone downloads it -----------------------------
# The signature verifies, it is the release key's (the certificate's SHA-256
# against the keystore's), and the APK is not debuggable and carries the
# manifest's version.
if [[ $RELEASE -eq 1 ]]; then
    CERTS="$(cmd //c "$APKSIGNER" verify --verbose --print-certs "$(win "$APK")" | tr -d '\r')"
    printf '%s\n' "$CERTS" | grep -E '^Verifies$|^Verified using v[0-9]+ scheme|^Signer #1 certificate (DN|SHA-256)'
    APK_SHA="$(printf '%s\n' "$CERTS" | sed -n 's/^Signer #1 certificate SHA-256 digest: //p' | tr 'A-F' 'a-f')"
    KEY_SHA="$(keytool -list -v -keystore "$(win "$RELEASE_KEYSTORE")" -storetype PKCS12 \
        -storepass:file "$(win "$RELEASE_PASS_FILE")" -alias "$RELEASE_ALIAS" \
        | tr -d '\r' | sed -n 's/^[[:space:]]*SHA256: //p' | tr -d ':' | tr 'A-F' 'a-f')"
    if [[ -z "$APK_SHA" || "$APK_SHA" != "$KEY_SHA" ]]; then
        echo "--release: the APK's certificate ($APK_SHA) is not the release key's ($KEY_SHA)" >&2
        exit 1
    fi
    echo "Certificate matches the release key: $KEY_SHA"
    BADGING="$("$BUILD_TOOLS/aapt2.exe" dump badging "$(win "$APK")" | tr -d '\r')"
    if printf '%s\n' "$BADGING" | grep -q 'application-debuggable'; then
        echo "--release: the APK is debuggable" >&2
        exit 1
    fi
    printf '%s\n' "$BADGING" | grep -E "^package: " | sed 's/ platformBuild.*//'
    [[ ! -e "$APK.idsig" ]] || { echo "--release: an .idsig was written beside the APK" >&2; exit 1; }
    echo "SHA-256 of the APK: $(sha256sum "$APK" | cut -d' ' -f1)"
fi

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
        # debuggable; --release refused flags above): from Android 11 the shell
        # cannot create files in the app's external directory.
        printf '%s\n' "$RUN_FLAGS" | "$ADB" -s "$SERIAL" exec-in run-as "$PACKAGE" sh -c 'cat > files/penumbra_args.txt'
    fi
    "$ADB" -s "$SERIAL" shell am start -n "$PACKAGE/com.ivancvetanovic.supersonic.SupersonicActivity"
fi
