# The port's suites.
#
# Each suite is one .cpp built by the engine's own rule, supersonic_add_test
# (engine/cmake/SupersonicTesting.cmake): the test harness, SupersonicCore,
# /W4 and add_test under the suite's own name. add_pn_test links the whole
# port, so a suite can drive the Eth runtime, the ported scripts or the layer.
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
