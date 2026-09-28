# PenumbraEth: the Ethanon 0.7.12 runtime the original's scripts ran on, as far
# as they depend on it. Deliberately free of the renderer: it produces a
# RenderSnapshot per frame and the layer draws that, so every suite can run a
# scene without a device.

file(GLOB PENUMBRA_ETH_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(PenumbraEth STATIC ${PENUMBRA_ETH_SOURCES})

# TinyXML 2.5 from the Ethanon 0.7.12 tree (zlib licence), which is what parsed
# the original's scenes - TIXML_ENCODING_LEGACY, bytes through untouched.
file(GLOB PENUMBRA_TINYXML_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tinyxml/*.cpp)
add_library(PenumbraTinyXml STATIC ${PENUMBRA_TINYXML_SOURCES})
target_include_directories(PenumbraTinyXml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tinyxml)
target_compile_definitions(PenumbraTinyXml PUBLIC TIXML_USE_STL)
if (MSVC)
    target_compile_options(PenumbraTinyXml PRIVATE /W0)
    target_compile_definitions(PenumbraTinyXml PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(PenumbraTinyXml PRIVATE -w)
endif()

# dr_mp3 (public domain / MIT-0), for the original's MP3s where the engine has
# no decoder: eth/SoundDecode.hpp says where that is. C, compiled once, its
# warnings the library's, like TinyXML's.
add_library(PenumbraDrMp3 STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/dr_mp3/dr_mp3.c)
target_compile_definitions(PenumbraDrMp3 PUBLIC DR_MP3_NO_STDIO)
if (MSVC)
    target_compile_options(PenumbraDrMp3 PRIVATE /W0)
else()
    target_compile_options(PenumbraDrMp3 PRIVATE -w)
endif()

target_link_libraries(PenumbraEth PUBLIC SupersonicCore PRIVATE PenumbraTinyXml PenumbraDrMp3)
target_include_directories(PenumbraEth PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# The original game, read in place and never written. Absolute, because a game
# on the engine moves its working directory to the engine checkout at startup
# and ctest runs the suites from build/.
get_filename_component(_penumbraRepo "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(PENUMBRA_ORIGINAL_DIR "${_penumbraRepo}/extracted/app"
    CACHE PATH "The original game's files (extracted from its installer), read-only")
target_compile_definitions(PenumbraEth PUBLIC PENUMBRA_ORIGINAL_DIR="${PENUMBRA_ORIGINAL_DIR}")

if (MSVC)
    target_compile_options(PenumbraEth PRIVATE /W4)
else()
    # The engine's own level for SupersonicCore (engine/CMakeLists.txt).
    target_compile_options(PenumbraEth PRIVATE -Wall -Wextra -Wpedantic)
endif()
