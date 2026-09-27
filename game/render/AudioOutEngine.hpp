#pragma once

// Eth::AudioOut (eth/Audio.hpp) over the engine's Supersonic::AudioEngine.
//
// The sample model itself - one voice per file, restart on replay, volumes
// that persist, everything released on LoadScene - is Eth::SampleBank's. This
// is only the device underneath it, and it keeps two differences from
// Audiere, which the bank has to live with:
//
//  - NOTHING STREAMS. 0.7.12 streamed music (docs/spec/30-ethanon-runtime.md
//    §4.2); the engine decodes a whole file to PCM on first use. fase.mp3 is
//    about 26 MB decoded, so its Load stalls the tick that asks (a scene load,
//    where a stall is least visible) - and, with the contract's UnloadAll on
//    every LoadScene, again on every checkpoint respawn. SetKeepDecodedClips
//    keeps the PCM across scene loads instead.
//  - A VOICE'S LOOP IS FIXED WHEN IT STARTS (XAudio2 takes the loop count with
//    the buffer), and there is no pause. The bank restarts a voice to change
//    its loop flag or to resume.
//
// Every voice is panned through SetVoiceParameters from its first frame, so a
// sample does not change loudness when a particle system starts panning it
// (the engine's constant-power law puts a centred voice at -3 dB; that is one
// global level, not a difference between sounds).

#include <cstddef>
#include <set>
#include <string>

#include <entt/entt.hpp>

#include "eth/Audio.hpp"

namespace Supersonic {
class AudioEngine;
}

namespace Penumbra::Render {

class AudioOutEngine final : public Eth::AudioOut {
public:
    AudioOutEngine() = default;
    // Does not touch the engine, which may already be gone. Call Detach from
    // the layer's OnDetach.
    ~AudioOutEngine() override = default;
    AudioOutEngine(const AudioOutEngine&) = delete;
    AudioOutEngine& operator=(const AudioOutEngine&) = delete;

    // The AudioEngine the app published in registry.ctx() (AudioSystem::Attach).
    // Without one (a suite on a bare registry) everything is a quiet no-op and
    // Load reports whether the file exists.
    void Attach(entt::registry& registry);
    void Attach(Supersonic::AudioEngine* engine);
    // Stops and unloads everything this loaded, then forgets the engine.
    void Detach();
    bool Available() const;

    // Keep decoded clips across UnloadAll (voices are still stopped). Off by
    // default: the contract is 0.7.12's release of everything on LoadScene.
    void SetKeepDecodedClips(bool keep) { m_keepDecoded = keep; }

    bool Load(const Eth::string& absolutePath, bool music) override;
    Eth::VoiceId Play(const Eth::string& absolutePath, bool loop, float volume, float pan) override;
    void Stop(Eth::VoiceId voice) override;
    void Set(Eth::VoiceId voice, float volume, float pan) override;
    bool IsPlaying(Eth::VoiceId voice) override;
    void UnloadAll() override;

    std::size_t LoadedCount() const { return m_loaded.size(); }

private:
    Supersonic::AudioEngine* m_engine = nullptr;
    std::set<std::string> m_loaded;   // every path handed to LoadClip, failures included
    bool m_keepDecoded = false;
};

} // namespace Penumbra::Render
