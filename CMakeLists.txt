# Penumbra and the Castle of Shadows - Enhanced, on the Supersonic Engine.
#
#   engine/   the engine, a git submodule pinned to a commit. Added as a
#             subproject, it builds SupersonicCore and the test harness and
#             nothing else: no editor, no script plugin, none of its suites.
#   game/     the port: eth/ (the Ethanon 0.7.12 runtime emulation), script/
#             (the original's AngelScript, ported to C++), render/ (drawing it
#             on the engine), the layer and the executable.
#   tests/    the port's suites, test_pn_*.
#
# The original game is read in place from extracted/app (PENUMBRA_ORIGINAL_DIR
# in game/eth/CMakeLists.txt, an absolute path one -D moves).
#
# Configure it as the engine's own build is configured (tools/build.bat does
# exactly this):
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=ON

cmake_minimum_required(VERSION 3.20)
# VERSION is the release's: the Windows resources carry it (game/windows),
# and tools/make_release.py checks that the Windows manifest and the Android
# manifest say the same.
project(Penumbra VERSION 1.0.8 LANGUAGES C CXX)
set(PENUMBRA_PRODUCT_NAME "Penumbra and the Castle of Shadows - Enhanced")

# Objective-C++ on Apple platforms: the entries in game/macos and game/ios ask
# the system for its language and the bundle's folders, and the engine's iOS
# backend is UIKit. Objective-C as well, for GLFW's Cocoa backend (.m), which
# would otherwise be compiled as Objective-C++. Enabled here, the directory
# above everything that compiles or links them (the engine enables them too,
# for its own build).
if (APPLE)
    enable_language(OBJC OBJCXX)

    # Apple's linker (Xcode 15 and later) warns about every library named twice
    # on a link line, which CMake writes whenever a target links SupersonicCore
    # both itself and through PenumbraGame, as the suites do: eighteen warnings a
    # build, about nothing. Silenced where the linker has the option.
    include(CheckLinkerFlag)
    check_linker_flag(CXX "LINKER:-no_warn_duplicate_libraries" PENUMBRA_LD_NO_WARN_DUPLICATES)
    if (PENUMBRA_LD_NO_WARN_DUPLICATES)
        add_link_options("LINKER:-no_warn_duplicate_libraries")
    endif()
endif()

# The engine's headers are C++20, and SupersonicCore says so on its interface;
# the port's own code is held to the same standard here, where the engine's
# directory settings do not reach.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/engine/CMakeLists.txt")
    message(FATAL_ERROR
        "engine/ is empty: the Supersonic Engine is a git submodule. Run\n"
        "  git submodule update --init\n"
        "(or clone with --recurse-submodules) and configure again.")
endif()
add_subdirectory(engine)

# Every executable carries the original's icon, version information and a
# manifest (game/windows). Smart App Control, enforcing on the development
# laptop, judges an unsigned executable by what it looks like until it has a
# reputation: freshly linked suites without these were refused at random, the
# game with them was not (docs/planning, step 3).
function(penumbra_windows_resources target description)
    if (NOT WIN32)
        return()
    endif()
    set(PENUMBRA_ICON "${PENUMBRA_ORIGINAL_DIR}/penumbra.ico")
    set(PENUMBRA_RC_NAME "${target}")
    set(PENUMBRA_RC_DESCRIPTION "${description}")
    set(PENUMBRA_RC_VERSION "${Penumbra_VERSION_MAJOR}.${Penumbra_VERSION_MINOR}.${Penumbra_VERSION_PATCH}.0")
    set(PENUMBRA_RC_VERSION_COMMAS "${Penumbra_VERSION_MAJOR},${Penumbra_VERSION_MINOR},${Penumbra_VERSION_PATCH},0")
    configure_file(${PENUMBRA_REPO_DIR}/game/windows/Penumbra.rc.in ${CMAKE_CURRENT_BINARY_DIR}/${target}.rc @ONLY)
    target_sources(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${target}.rc ${PENUMBRA_REPO_DIR}/game/windows/Penumbra.manifest)
endfunction()
set(PENUMBRA_REPO_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(game)

option(PENUMBRA_BUILD_TESTS "Build the port's test_pn_* suites" ON)
if (PENUMBRA_BUILD_TESTS)
    # The engine enables testing only for its own suites, which are off here.
    enable_testing()
    add_subdirectory(tests)
endif()
