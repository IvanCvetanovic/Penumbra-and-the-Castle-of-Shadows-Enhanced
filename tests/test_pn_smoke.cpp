// The build itself: the port links against the engine, and the original's
// files are where the build was told they are.
#include <filesystem>

#include "TestHarness.hpp"

int main() {
    if (!std::filesystem::exists(PENUMBRA_ORIGINAL_DIR "/main.as")) {
        std::printf("SKIP: the original is not at %s\n", PENUMBRA_ORIGINAL_DIR);
        return 77;
    }
    CHECK(std::filesystem::exists(PENUMBRA_ORIGINAL_DIR "/scenes/level1.esc"));
    CHECK(std::filesystem::exists(PENUMBRA_ORIGINAL_DIR "/data.enml"));
    return test::summary("test_pn_smoke", 2);
}
