#pragma once

// test_pn_all's table of suites. Each generated wrapper (WrapSuite.cmake) adds
// its suite from a static initializer; RunAll.cpp reads the table in main(),
// when every initializer has run, and sorts it by name.
//
// The table is a function-local static so that it exists before the first
// wrapper's initializer reaches for it, in whatever order the linker lays the
// wrappers' initializers out. Nothing here is generated, so the runner compiles
// (tools/check.bat) without a build tree.

#include <vector>

namespace PnAll {

struct Suite {
    const char* name;   // the suite's own name, e.g. "test_pn_boot"
    int (*run)();       // its main(), in namespace PnSuites::<name>
};

inline std::vector<Suite>& Suites() {
    static std::vector<Suite> suites;
    return suites;
}

struct Registration {
    Registration(const char* name, int (*run)()) { Suites().push_back({name, run}); }
};

} // namespace PnAll
