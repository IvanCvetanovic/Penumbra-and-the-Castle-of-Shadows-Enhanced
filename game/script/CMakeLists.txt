# PenumbraScript: the original's 25 AngelScript files, ported to C++ one file
# per file against eth/Eth.hpp. Script.hpp declares every global and function
# they share, as main.as's #includes made them one module.

file(GLOB PENUMBRA_SCRIPT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(PenumbraScript STATIC ${PENUMBRA_SCRIPT_SOURCES})
target_link_libraries(PenumbraScript PUBLIC PenumbraEth)
target_include_directories(PenumbraScript PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

if (MSVC)
    target_compile_options(PenumbraScript PRIVATE /W4)
else()
    # The engine's own level for SupersonicCore (engine/CMakeLists.txt).
    target_compile_options(PenumbraScript PRIVATE -Wall -Wextra -Wpedantic)
endif()
