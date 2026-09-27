// Every sound the original ships decodes through the engine: 19 Ogg Vorbis
// files (stb_vorbis, engine abb9e8a) and 16 MP3s (Media Foundation), each to
// 16-bit PCM of a plausible length. The game's samples are all played by name
// from soundfx/ (setupScene.as:114-161, the .ent <SoundEffect>s), so a file
// that did not decode would be a silent sword, jump or boss.

#include <cstdio>
#include <filesystem>
#include <string>

#include "TestHarness.hpp"

#include "core/AudioClip.hpp"

int main() {
    const std::filesystem::path dir = std::filesystem::path(PENUMBRA_ORIGINAL_DIR) / "soundfx";
    if (!std::filesystem::exists(dir)) {
        std::printf("SKIP: the original is not at %s\n", PENUMBRA_ORIGINAL_DIR);
        return 77;
    }

    int ogg = 0;
    int mp3 = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        const std::string ext = entry.path().extension().string();
        if (ext != ".ogg" && ext != ".mp3") continue;
        Supersonic::AudioClip clip;
        std::string error;
        const bool ok = Supersonic::AudioClip::Load(entry.path().string(), clip, error);
        CHECK_MSG(ok, entry.path().filename().string() + ": " + error);
        if (!ok) continue;
        (ext == ".ogg" ? ogg : mp3)++;
        CHECK(clip.channels == 1 || clip.channels == 2);
        CHECK(clip.sampleRate == 22050 || clip.sampleRate == 44100 || clip.sampleRate == 48000);
        CHECK_EQ(clip.bitsPerSample, 16);
        const double seconds = static_cast<double>(clip.pcm.size()) / (2.0 * clip.channels * clip.sampleRate);
        // The shortest effect is a fraction of a second, the longest the level
        // music at a few minutes.
        CHECK_MSG(seconds > 0.05 && seconds < 600.0, entry.path().filename().string() + " lasts " + std::to_string(seconds) + " s");
        std::printf("  %-22s %u ch %5u Hz %8.2f s\n", entry.path().filename().string().c_str(), clip.channels,
                    clip.sampleRate, seconds);
    }
    CHECK_EQ(ogg, 19);
    CHECK_EQ(mp3, 16);
    return test::summary("test_pn_audio", 100);
}
