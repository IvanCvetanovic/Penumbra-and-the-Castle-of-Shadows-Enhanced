// Every sound the original ships decodes on this platform: 19 Ogg Vorbis
// files (the engine's stb_vorbis, engine abb9e8a) and 16 MP3s (the engine's
// Media Foundation on Windows, the port's dr_mp3 elsewhere: eth/SoundDecode.hpp),
// each to 16-bit PCM of a plausible length. The game's samples are all played
// by name from soundfx/ (setupScene.as:114-161, the .ent <SoundEffect>s), so a
// file that did not decode would be a silent sword, jump or boss.
//
// And dr_mp3 agrees with whatever decoded each MP3 here - on Windows that is
// Media Foundation, so the decoder the other platforms use is measured against
// the one the game was tuned with: the same channels and rate, the same
// length give or take the encoder delay the two treat differently.

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "TestHarness.hpp"

#include "core/AudioClip.hpp"
#include "eth/SoundDecode.hpp"

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
        const bool ok = Penumbra::Eth::LoadSound(entry.path().string(), clip, error);
        CHECK_MSG(ok, entry.path().filename().string() + ": " + error);
        if (!ok) continue;
        (ext == ".ogg" ? ogg : mp3)++;
        if (ext == ".mp3") {
            Supersonic::AudioClip ours;
            const bool decoded = Penumbra::Eth::DecodeMp3(entry.path().string(), ours, error);
            CHECK_MSG(decoded, entry.path().filename().string() + ": " + error);
            if (decoded) {
                CHECK_EQ(ours.channels, clip.channels);
                CHECK_EQ(ours.sampleRate, clip.sampleRate);
                CHECK_EQ(ours.bitsPerSample, 16);
                CHECK_MSG(std::fabs(ours.durationSeconds() - clip.durationSeconds()) < 0.1f,
                          entry.path().filename().string() + ": dr_mp3 " + std::to_string(ours.durationSeconds()) +
                              " s, the platform's decoder " + std::to_string(clip.durationSeconds()) + " s");
            }
        }
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
