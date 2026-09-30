# The port's suites.
#
# Each suite is one .cpp built by the engine's own rule, supersonic_add_test
# (engine/cmake/SupersonicTesting.cmake): the test harness, SupersonicCore,
# /W4 and add_test under the suite's own name. add_pn_test links the whole
# port, so a suite can drive the Eth runtime, the ported scripts or the layer.
#
# PENUMBRA_TESTS_DATA_DIR is tests/data, the suites' own data files (the
# language rooms, l10n_rooms.json); test_pn_all gets it too.
#
# SKIPS_WITH_77 marks a suite that reads the original's files from
# PENUMBRA_ORIGINAL_DIR: a machine without them exits 77 before its first
# check, which ctest reports SKIPPED - never a pass.
#
# A suite links the runtime and the ported scripts (PenumbraScript, which
# brings PenumbraEth); one named test_pn_render_* or test_pn_layer_* links the
# whole game (PenumbraGame), so the runtime's suites build while render/ is
# still being written.
function(add_pn_test name)
    cmake_parse_arguments(PARSE_ARGV 1 PN "SKIPS_WITH_77;GAME" "" "")
    if (PN_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "add_pn_test(${name}): unknown argument ${PN_UNPARSED_ARGUMENTS}")
    endif()
    supersonic_add_test(${name})
    penumbra_windows_resources(${name} "Penumbra test suite ${name}")
    target_compile_definitions(${name} PRIVATE PENUMBRA_TESTS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
    if (PN_GAME)
        target_link_libraries(${name} PRIVATE PenumbraGame)
    else()
        target_link_libraries(${name} PRIVATE PenumbraScript)
    endif()
    if (PN_SKIPS_WITH_77)
        set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77)
    endif()
endfunction()

file(GLOB _pnSuites RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_pn_*.cpp)
foreach(_suite ${_pnSuites})
    get_filename_component(_name ${_suite} NAME_WE)
    if (_name MATCHES "^test_pn_(render|layer)")
        add_pn_test(${_name} SKIPS_WITH_77 GAME)
    else()
        add_pn_test(${_name} SKIPS_WITH_77)
    endif()
endforeach()

# test_pn_all: every suite above in ONE executable, beside the suites' own
# executables (which stay, and which ctest keeps registering). Smart App
# Control judges each freshly linked executable when it is first launched, and
# a build that relinks every suite gives it a chance per suite to refuse one;
# launching test_pn_all instead is one judgement per build.
#
# Each suite is compiled from a wrapper that all/WrapSuite.cmake writes at
# BUILD time, whenever the suite changes (a wrapper written once at configure
# time would go stale the moment a suite gained an #include): the suite's own
# #includes at global scope, then the suite itself inside namespace
# PnSuites::<name>, so its main() and helpers cannot meet another suite's. The
# runner, all/RunAll.cpp, starts each suite in a child process of this same
# executable (the ported script's globals outlive a Machine, so three suites
# that boot the real game cannot share a process); `--suite <name>` runs one
# in-process, exactly as its own executable does.
#
# Not registered with ctest: ctest already runs every suite through its own
# executable, and a second registration would run each suite twice. A developer
# launches build/tests/test_pn_all once per build instead of ctest.
#
# Not supersonic_add_test either, which builds exactly one <name>.cpp and calls
# add_test; its other settings are repeated here: the harness, SupersonicCore
# (through PenumbraGame, which the render suites need), /W4.
set(_pnAllWrappers ${CMAKE_CURRENT_BINARY_DIR}/pn_all_wrappers)   # never "test_pn_all": the executable's name off Windows
set(_pnAllSources ${CMAKE_CURRENT_SOURCE_DIR}/all/RunAll.cpp ${CMAKE_CURRENT_SOURCE_DIR}/all/SuiteRegistry.hpp)
foreach(_suite ${_pnSuites})
    get_filename_component(_name ${_suite} NAME_WE)
    set(_wrapper ${_pnAllWrappers}/${_name}.cpp)
    add_custom_command(
        OUTPUT ${_wrapper}
        COMMAND ${CMAKE_COMMAND} -DSUITE=${CMAKE_CURRENT_SOURCE_DIR}/${_suite} -DOUT=${_wrapper}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/all/WrapSuite.cmake
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_suite} ${CMAKE_CURRENT_SOURCE_DIR}/all/WrapSuite.cmake
        COMMENT "test_pn_all: wrapping ${_name}"
        VERBATIM)
    list(APPEND _pnAllSources ${_wrapper})
endforeach()
add_executable(test_pn_all ${_pnAllSources})
penumbra_windows_resources(test_pn_all "Penumbra test suites, all in one")
target_link_libraries(test_pn_all PRIVATE Supersonic::TestHarness SupersonicCore PenumbraGame)
target_compile_definitions(test_pn_all PRIVATE PENUMBRA_TESTS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
if (MSVC)
    target_compile_options(test_pn_all PRIVATE /W4)
else()
    target_compile_options(test_pn_all PRIVATE -Wall -Wextra)
endif()
