# The port, as four libraries and an executable:
#
#   PenumbraEth     eth/     the Ethanon 0.7.12 runtime emulation: scene and entity
#                            files, buckets, custom data, callbacks, the frame
#                            order, particles, input states, the sample bank.
#                            No renderer; the tests link it bare.
#   PenumbraScript  script/  the original's AngelScript ported to C++, one .cpp
#                            per .as, against eth/Eth.hpp.
#   PenumbraGame    render/ + PenumbraLayer: the layer that runs one Ethanon
#                            frame per engine tick and draws the snapshot it
#                            leaves. A library so a test can attach it to a bare
#                            registry.
#   Penumbra        main.cpp only.

add_subdirectory(eth)
add_subdirectory(script)

file(GLOB PENUMBRA_RENDER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/render/*.cpp)
add_library(PenumbraGame STATIC
    PenumbraLayer.cpp
    ${PENUMBRA_RENDER_SOURCES}
)
target_link_libraries(PenumbraGame PUBLIC SupersonicCore PenumbraEth PenumbraScript)
target_include_directories(PenumbraGame PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Where the port's own data lives (translations, settings defaults). Read from
# the source tree, like the original's files.
set(PENUMBRA_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data"
    CACHE PATH "The port's own JSON data (game/data)")
target_compile_definitions(PenumbraGame PUBLIC PENUMBRA_DATA_DIR="${PENUMBRA_DATA_DIR}")

add_executable(Penumbra main.cpp)
# The icon (the original's, read in place), version information and the
# manifest (per-monitor DPI aware); the top-level CMakeLists.txt says why.
penumbra_windows_resources(Penumbra "Penumbra and the Castle of Shadows - Enhanced")
target_link_libraries(Penumbra PRIVATE PenumbraGame)

foreach(target PenumbraGame Penumbra)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endforeach()
