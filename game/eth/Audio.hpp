#pragma once

// Ethanon's sample model over whatever plays sound.
//
// 0.7.12 keeps ONE Audiere stream per file, keyed by basename
// (docs/spec/30-ethanon-runtime.md §4.2): PlaySample on a playing sample
// restarts it (no overlap), LoopSample sets its repeat flag, SetSampleVolume is
// linear and clamped to 0..1 and persists, StopSample rewinds, and every
// LoadScene stops and forgets all of them. A particle system's <SoundEffect>
// is the SAME sample object, and while that system has live particles its
// HandleSoundPlayback overwrites the sample's volume, pan and loop every frame.
//
// SampleBank is that model, pure. It talks to an AudioOut, which the layer
// implements over Supersonic::AudioEngine (and a suite implements as a
// recorder).

#include <cstdint>
#include <map>
#include <string>

#include "eth/EthTypes.hpp"

namespace Penumbra::Eth {

using VoiceId = std::uint64_t;   // 0 = no voice

class AudioOut {
public:
    virtual ~AudioOut() = default;
    // Decode (or open for streaming) the file at an absolute path. False when it
    // cannot be read. Idempotent.
    virtual bool Load(const string& absolutePath, bool music) = 0;
    virtual VoiceId Play(const string& absolutePath, bool loop, float volume, float pan) = 0;
    virtual void Stop(VoiceId voice) = 0;
    virtual void Set(VoiceId voice, float volume, float pan) = 0;
    virtual bool IsPlaying(VoiceId voice) = 0;
    // Forget every decoded clip (a scene load in 0.7.12 released them all).
    virtual void UnloadAll() = 0;
};

class SampleBank {
public:
    // `soundRoot` is where "soundfx/x.ogg" resolves: the original's root.
    explicit SampleBank(string gameRoot) : m_gameRoot(std::move(gameRoot)) {}

    void SetOutput(AudioOut* out) { m_out = out; }
    // An enhancement: master volumes for music and effects (0..1), applied on
    // top of the scripts' own volumes.
    void SetMasterVolumes(float music, float effects);

    // The script API. `path` is what the script passed ("soundfx/fase.mp3");
    // samples are keyed by its basename, as 0.7.12 keyed them.
    bool LoadMusic(const string& path);
    bool LoadSoundEffect(const string& path);
    bool PlaySample(const string& path);
    bool LoopSample(const string& path, bool loop);
    bool StopSample(const string& path);
    bool PauseSample(const string& path);
    bool SetSampleVolume(const string& path, float volume);
    bool SetSamplePan(const string& path, float pan);
    bool SampleExists(const string& path) const;
    bool IsSamplePlaying(const string& path);

    // A particle system's <SoundEffect> (a bare file name in soundfx/): loads it
    // if needed and plays it from the start, as StartSFX did on AddEntity.
    void StartEffect(const string& fileName, float volume);
    // Apply a particle system's HandleSoundPlayback decision to its sample.
    void ApplyEffect(const string& fileName, bool stop, bool loop, float volume, float pan);

    // LoadScene: stop and release every sample.
    void ReleaseAll();

private:
    struct Sample {
        string absolutePath;
        bool music = false;
        bool loop = false;
        float volume = 1.0f;
        float pan = 0.0f;
        VoiceId voice = 0;
    };
    static string Key(const string& path);
    Sample* Find(const string& path);

    string m_gameRoot;
    AudioOut* m_out = nullptr;
    std::map<string, Sample> m_samples;
    float m_musicVolume = 1.0f;
    float m_effectsVolume = 1.0f;
};

} // namespace Penumbra::Eth
