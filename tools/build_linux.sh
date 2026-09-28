#!/usr/bin/env bash
# Build the Penumbra port on Linux: the counterpart of tools/build.bat. Written to run on macOS too (no GNU-only
# options), which is untested.
#
# Usage:  bash tools/build_linux.sh [--build-dir DIR] [--jobs N] [--clang] [--test] [cmake --build args...]
#   e.g.  bash tools/build_linux.sh --target Penumbra
#         bash tools/build_linux.sh --test             # build everything, then run test_pn_all once
#         bash tools/build_linux.sh -- -k 0            # keep going past a failing file (ninja's -k)
# --clang configures a NEW build directory with clang/clang++ (default: $BUILD-clang); an existing
# directory keeps the compiler it was configured with.
#
# From Windows, through WSL (the sources stay on the Windows drive, the build does not):
#   wsl -d Ubuntu-24.04 -- bash /mnt/c/.../tools/build_linux.sh --test
#
# The build directory must be on a Linux filesystem, never the repository's build/ (that one is the
# Windows build) and never under /mnt/c, where every file operation crosses into Windows and a build
# takes many times as long. Default: $PENUMBRA_LINUX_BUILD, else ~/pn-build-linux.
#
# The first run - or the first after the build directory is deleted - also configures it, as the
# engine's CI configures its Linux job: Ninja, Release, Vulkan validation compiled in (the layers
# are used only when installed), GLFW without Wayland (GLFW 3.4 refuses to configure without the
# whole Wayland toolchain; X11 is enough). A configured directory is left as it is.
#
# Jobs are bounded (default 6): an unbounded -j compiles dozens of translation units that each pull
# in vulkan.hpp at once, and the OOM killer ends the build with "Killed signal terminated program
# cc1plus", which reads like a compiler crash.
#
# Packages (Ubuntu 24.04), as the engine's CI installs them plus the toolchain and a headless run:
#   cmake ninja-build g++ (or clang) pkg-config libvulkan-dev libx11-dev libxrandr-dev
#   libxinerama-dev libxcursor-dev libxi-dev libxkbcommon-dev libasound2-dev
#   xvfb mesa-vulkan-drivers vulkan-tools        (headless captures on lavapipe, see CLAUDE.md)
# No glslc is needed: without one the engine uses its committed SPIR-V, and with one a build of the
# engine's Shaders target would rewrite those blobs inside the submodule.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD=""
JOBS=6
COMPILER=gcc
RUN_TESTS=0
BUILD_ARGS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) BUILD="$2"; shift 2 ;;
        --jobs) JOBS="$2"; shift 2 ;;
        --clang) COMPILER=clang; shift ;;
        --test) RUN_TESTS=1; shift ;;
        *) BUILD_ARGS+=("$1"); shift ;;
    esac
done
if [ -z "$BUILD" ]; then
    BUILD="${PENUMBRA_LINUX_BUILD:-$HOME/pn-build-linux}"
    [ "$COMPILER" = clang ] && BUILD="$BUILD-clang"
fi

if [ ! -f "$REPO/engine/CMakeLists.txt" ]; then
    echo "engine/ is empty: the Supersonic Engine is a submodule. Run  git submodule update --init  first." >&2
    exit 1
fi
case "$BUILD" in
    "$REPO"/build|"$REPO"/build/*) echo "$BUILD is the Windows build; pass another --build-dir" >&2; exit 1 ;;
esac

if [ ! -f "$BUILD/CMakeCache.txt" ]; then
    if [ "$COMPILER" = clang ]; then
        CC_ARGS=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++)
    else
        CC_ARGS=(-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++)
    fi
    cmake -S "$REPO" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=ON \
        -DGLFW_BUILD_WAYLAND=OFF "${CC_ARGS[@]}"
fi

cmake --build "$BUILD" --parallel "$JOBS" ${BUILD_ARGS[@]+"${BUILD_ARGS[@]}"}

# The engine's shaders beside the executable, as tools/package.bat lays them out. The engine then
# anchors its working directory to build/game (ChooseAssetRoot: the executable's folder holding
# assets/shaders wins over the baked engine checkout), so a Linux run writes its
# cache/pipeline_cache.bin there - never into engine/cache on the Windows drive, which the Windows
# build's runs use.
if [ -d "$BUILD/game" ]; then
    mkdir -p "$BUILD/game/assets/shaders"
    cp "$REPO"/engine/assets/shaders/*.spv "$BUILD/game/assets/shaders/"
fi

if [ "$RUN_TESTS" = 1 ]; then
    # One launch, as on Windows: the runner starts each suite in a child process of itself.
    "$BUILD/tests/test_pn_all"
fi
