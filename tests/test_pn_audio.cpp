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
//
// ENHANCEMENT E29: and the sample bank's one exception to "a scene load releases every
// sample" - a sample asked to be kept (KeepOnNextLoad) goes on through the next load, the
// same voice, and only through that one - against a stand-in for the device whose
// UnloadAll stops every voice, as AudioOutEngine's does.

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>

#include "TestHarness.hpp"

#include "core/AudioClip.hpp"
#include "eth/Audio.hpp"
#include "eth/SoundDecode.hpp"

namespace {   // E29

namespace Eth = Penumbra::Eth;

// What the speakers would be told: each voice, whether it still sounds, and the volume it was
// last given. UnloadAll stops them all, as the engine-backed output does (StopVoicesUsing).
class Device final : public Eth::AudioOut {
public:
    struct Voice {
        std::string file;
        bool loop = false;
        float volume = 1.0f;
        bool alive = true;
    };
    bool Load(const Eth::string& absolutePath, const bool music) override {
        (void)music;
        (void)absolutePath;
        ++loads;
        return true;
    }
    Eth::VoiceId Play(const Eth::string& absolutePath, const bool loop, const float volume, const float pan) override {
        (void)pan;
        voices[++next] = Voice{absolutePath, loop, volume, true};
        return next;
    }
    void Stop(const Eth::VoiceId voice) override {
        const auto it = voices.find(voice);
        if (it != voices.end()) it->second.alive = false;
    }
    void Set(const Eth::VoiceId voice, const float volume, const float pan) override {
        (void)pan;
        const auto it = voices.find(voice);
        if (it != voices.end()) it->second.volume = volume;
    }
    bool IsPlaying(const Eth::VoiceId voice) override {
        const auto it = voices.find(voice);
        return it != voices.end() && it->second.alive;
    }
    void UnloadAll() override {
        ++unloads;
        for (auto& entry : voices) entry.second.alive = false;
    }
    std::map<Eth::VoiceId, Voice> voices;
    Eth::VoiceId next = 0;
    int loads = 0;
    int unloads = 0;
};

void TestKeepOnNextLoad() {
    Device device;
    Eth::SampleBank bank(PENUMBRA_ORIGINAL_DIR);
    bank.SetOutput(&device);

    // The menu's song, looping at a lowered volume, and a one-shot effect.
    CHECK(bank.LoadMusic("soundfx/menu.mp3"));
    CHECK(bank.LoopSample("soundfx/menu.mp3", true));
    CHECK(bank.PlaySample("soundfx/menu.mp3"));
    const Eth::VoiceId song = device.next;
    CHECK(bank.SetSampleVolume("soundfx/menu.mp3", 0.25f));
    CHECK(bank.LoadSoundEffect("soundfx/help.mp3"));
    CHECK(bank.PlaySample("soundfx/help.mp3"));
    const Eth::VoiceId effect = device.next;
    CHECK(device.IsPlaying(song) && device.IsPlaying(effect));

    // Nothing to keep: no such sample, and one that was loaded and never played.
    CHECK(!bank.KeepOnNextLoad("soundfx/none.mp3"));
    CHECK(bank.LoadSoundEffect("soundfx/fail.ogg"));
    CHECK(!bank.KeepOnNextLoad("soundfx/fail.ogg"));

    // Without the request a load releases everything, with the device's UnloadAll.
    {
        Device plain;
        Eth::SampleBank other(PENUMBRA_ORIGINAL_DIR);
        other.SetOutput(&plain);
        CHECK(other.LoadMusic("soundfx/menu.mp3") && other.PlaySample("soundfx/menu.mp3"));
        other.ReleaseAll();
        CHECK(plain.voices.begin()->second.alive == false);
        CHECK_EQ(plain.unloads, 1);
        CHECK(!other.SampleExists("soundfx/menu.mp3"));
        other.SetOutput(nullptr);
    }

    // The request: the song goes through the load, whole; the effect does not.
    CHECK(bank.KeepOnNextLoad("soundfx/menu.mp3"));
    const std::size_t played = device.voices.size();
    bank.ReleaseAll();
    CHECK(device.IsPlaying(song));
    CHECK(!device.IsPlaying(effect));
    CHECK_EQ(device.unloads, 0);                      // UnloadAll would have stopped the song
    CHECK_EQ(device.voices.size(), played);           // and nothing was started again
    CHECK(bank.SampleExists("soundfx/menu.mp3"));
    CHECK(!bank.SampleExists("soundfx/help.mp3"));
    CHECK(!bank.SampleExists("soundfx/fail.ogg"));
    CHECK(bank.IsSamplePlaying("soundfx/menu.mp3"));
    CHECK(bank.LoadMusic("soundfx/menu.mp3"));        // the next scene's preLoop finds it: a no-op
    CHECK_EQ(device.loads, 3);                        // menu, help, fail: no fourth
    CHECK(device.voices.at(song).loop);
    CHECK_EQ(device.voices.at(song).volume, 0.25f);   // its volume came with it

    // It is still the bank's: the music volume reaches it, and the script's volume replaces its own.
    bank.SetMasterVolumes(0.5f, 1.0f);
    CHECK_EQ(device.voices.at(song).volume, 0.125f);
    CHECK(bank.SetSampleVolume("soundfx/menu.mp3", 1.0f));
    CHECK_EQ(device.voices.at(song).volume, 0.5f);
    bank.SetMasterVolumes(1.0f, 1.0f);
    CHECK_EQ(device.voices.at(song).volume, 1.0f);

    // The request was for that load only: the next one releases it.
    bank.ReleaseAll();
    CHECK(!device.IsPlaying(song));
    CHECK_EQ(device.unloads, 1);
    CHECK(!bank.SampleExists("soundfx/menu.mp3"));

    // A sample that is not sounding when the load comes (a one-shot that ended, a stopped one) is
    // released as usual, and the request dies with it: a new sample of the same name starts clean.
    CHECK(bank.LoadMusic("soundfx/menu.mp3"));
    CHECK(bank.PlaySample("soundfx/menu.mp3"));
    const Eth::VoiceId ended = device.next;
    CHECK(bank.KeepOnNextLoad("soundfx/menu.mp3"));
    device.Stop(ended);                               // it ended by itself
    bank.ReleaseAll();
    CHECK(!bank.SampleExists("soundfx/menu.mp3"));
    CHECK_EQ(device.unloads, 2);
    CHECK(bank.LoadMusic("soundfx/menu.mp3"));
    CHECK(bank.PlaySample("soundfx/menu.mp3"));
    const Eth::VoiceId fresh = device.next;
    bank.ReleaseAll();                                // nothing asked for this one
    CHECK(!device.IsPlaying(fresh));
    CHECK_EQ(device.unloads, 3);

    // No output, no voice: nothing can be kept, and a load still empties the bank. (Without an output
    // a load only asks whether the file exists, hence the original's own root.)
    Eth::SampleBank silent(PENUMBRA_ORIGINAL_DIR);
    CHECK(silent.LoadMusic("soundfx/menu.mp3"));
    CHECK(silent.PlaySample("soundfx/menu.mp3"));
    CHECK(!silent.KeepOnNextLoad("soundfx/menu.mp3"));
    silent.ReleaseAll();
    CHECK(!silent.SampleExists("soundfx/menu.mp3"));
    bank.SetOutput(nullptr);
}

} // namespace   // E29

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
    TestKeepOnNextLoad();   // E29
    return test::summary("test_pn_audio", 100);
}
