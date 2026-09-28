// Ethanon 0.7.12's sample model (ETHEngine.cpp:1204-1318, ETHResourceManager.cpp:
// 124-200, GameSpaceLib's Audiere wrapper at DLL@0x100018a0..0x10001ae0), over
// an AudioOut. docs/spec/30-ethanon-runtime.md §4.2.
//
// Two things the AudioOut interface cannot say, and how they are carried:
//  - Changing the loop flag of a voice that is already playing. Audiere set the
//    stream's repeat flag in place; here the voice is restarted with the new
//    flag. The scripts only ever call LoopSample right after PlaySample in the
//    same frame (menu.as:45-47, setupScene.as:46-48, :233-236, events.as:55),
//    where the restart is inaudible.
//  - Pause. PauseSample stops the voice, and the next PlaySample starts it from
//    the beginning rather than resuming. The scripts never pause.

#include "eth/Audio.hpp"

#include <algorithm>
#include <filesystem>

#include "core/Log.hpp"
#include "eth/Paths.hpp"

namespace Penumbra::Eth {

namespace {

float Clamp01(const float v) {
    if (!(v == v)) return 0.0f;
    return std::clamp(v, 0.0f, 1.0f);
}

float ClampPan(const float v) {
    if (!(v == v)) return 0.0f;
    return std::clamp(v, -1.0f, 1.0f);
}

} // namespace

string SampleBank::Key(const string& path) {
    // ETHGlobal::GetFileName (ETHCommon.h:280-294): the part after the last
    // slash of either kind.
    const auto slash = path.find_last_of("/\\");
    return slash == string::npos ? path : path.substr(slash + 1);
}

SampleBank::Sample* SampleBank::Find(const string& path) {
    const auto it = m_samples.find(Key(path));
    return it == m_samples.end() ? nullptr : &it->second;
}

void SampleBank::SetMasterVolumes(const float music, const float effects) {
    m_musicVolume = Clamp01(music);
    m_effectsVolume = Clamp01(effects);
    if (m_out == nullptr) return;
    for (auto& [ignoredKey, sample] : m_samples) {
        if (sample.voice != 0) {
            m_out->Set(sample.voice, sample.volume * (sample.music ? m_musicVolume : m_effectsVolume), sample.pan);
        }
    }
}

bool SampleBank::LoadMusic(const string& path) {
    const string key = Key(path);
    // Loading a basename already loaded is a no-op (ETHResourceManager.cpp:176-182).
    if (m_samples.count(key) != 0) return true;
    const string absolutePath = ResolveUnder(m_gameRoot, path);
    const bool loaded = m_out != nullptr ? m_out->Load(absolutePath, true)
                                         : std::filesystem::exists(std::filesystem::path(absolutePath));
    if (!loaded) {
        SUPERSONIC_LOG_WARN("Penumbra") << "Could not load the file: " << path;
        return false;
    }
    Sample sample;
    sample.absolutePath = absolutePath;
    sample.music = true;
    m_samples.emplace(key, sample);
    return true;
}

bool SampleBank::LoadSoundEffect(const string& path) {
    const string key = Key(path);
    if (m_samples.count(key) != 0) return true;
    const string absolutePath = ResolveUnder(m_gameRoot, path);
    const bool loaded = m_out != nullptr ? m_out->Load(absolutePath, false)
                                         : std::filesystem::exists(std::filesystem::path(absolutePath));
    if (!loaded) {
        SUPERSONIC_LOG_WARN("Penumbra") << "Could not load the file: " << path;
        return false;
    }
    Sample sample;
    sample.absolutePath = absolutePath;
    sample.music = false;
    m_samples.emplace(key, sample);
    return true;
}

bool SampleBank::PlaySample(const string& path) {
    Sample* sample = Find(path);
    if (sample == nullptr) {
        // PlaySample never loads (ETHEngine.cpp:1228-1238).
        SUPERSONIC_LOG_WARN("Penumbra") << "File not found: " << path;
        return false;
    }
    if (m_out == nullptr) return true;
    // One voice per file: a sample already playing is reset and restarted,
    // never overlapped (DLL@0x100018a0).
    if (sample->voice != 0) m_out->Stop(sample->voice);
    const float master = sample->music ? m_musicVolume : m_effectsVolume;
    sample->voice = m_out->Play(sample->absolutePath, sample->loop, sample->volume * master, sample->pan);
    return true;
}

bool SampleBank::LoopSample(const string& path, const bool loop) {
    Sample* sample = Find(path);
    if (sample == nullptr) {
        SUPERSONIC_LOG_WARN("Penumbra") << "File not found: " << path;
        return false;
    }
    const bool changed = sample->loop != loop;
    sample->loop = loop;
    if (changed && m_out != nullptr && sample->voice != 0 && m_out->IsPlaying(sample->voice)) {
        m_out->Stop(sample->voice);
        const float master = sample->music ? m_musicVolume : m_effectsVolume;
        sample->voice = m_out->Play(sample->absolutePath, sample->loop, sample->volume * master, sample->pan);
    }
    return true;
}

bool SampleBank::StopSample(const string& path) {
    Sample* sample = Find(path);
    if (sample == nullptr) {
        SUPERSONIC_LOG_WARN("Penumbra") << "File not found: " << path;
        return false;
    }
    // Stop rewinds: the next Play starts from the beginning.
    if (m_out != nullptr && sample->voice != 0) m_out->Stop(sample->voice);
    sample->voice = 0;
    return true;
}

bool SampleBank::PauseSample(const string& path) {
    Sample* sample = Find(path);
    if (sample == nullptr) {
        SUPERSONIC_LOG_WARN("Penumbra") << "File not found: " << path;
        return false;
    }
    if (m_out != nullptr && sample->voice != 0) m_out->Stop(sample->voice);
    sample->voice = 0;
    return true;
}

bool SampleBank::SetSampleVolume(const string& path, const float volume) {
    Sample* sample = Find(path);
    if (sample == nullptr) {
        SUPERSONIC_LOG_WARN("Penumbra") << "File not found: " << path;
        return false;
    }
    // Linear, clamped to [0, 1] (ETHEngine.cpp:1284), and it persists.
    sample->volume = Clamp01(volume);
    if (m_out != nullptr && sample->voice != 0) {
        m_out->Set(sample->voice, sample->volume * (sample->music ? m_musicVolume : m_effectsVolume), sample->pan);
    }
    return true;
}

bool SampleBank::SetSamplePan(const string& path, const float pan) {
    Sample* sample = Find(path);
    if (sample == nullptr) {
        SUPERSONIC_LOG_WARN("Penumbra") << "File not found: " << path;
        return false;
    }
    sample->pan = ClampPan(pan);
    if (m_out != nullptr && sample->voice != 0) {
        m_out->Set(sample->voice, sample->volume * (sample->music ? m_musicVolume : m_effectsVolume), sample->pan);
    }
    return true;
}

bool SampleBank::SampleExists(const string& path) const {
    return m_samples.count(Key(path)) != 0;
}

bool SampleBank::IsSamplePlaying(const string& path) {
    Sample* sample = Find(path);
    if (sample == nullptr) {
        SUPERSONIC_LOG_WARN("Penumbra") << "File not found: " << path;
        return false;
    }
    // A repeating sample stays "playing" while it loops.
    return m_out != nullptr && sample->voice != 0 && m_out->IsPlaying(sample->voice);
}

void SampleBank::StartEffect(const string& fileName, const float volume) {
    // StartSFX (ETHRenderEntity.cpp:1226-1237) on the sample the particle system
    // found with GetPointer(..., "soundfx\\", GSST_SOUND_EFFECT), which loads it
    // on first use (ETHRenderEntity.cpp:365-371).
    if (fileName.empty()) return;
    if (Find(fileName) == nullptr && !LoadSoundEffect("soundfx/" + Key(fileName))) return;
    Sample* sample = Find(fileName);
    // The entity's volume is what HandleSoundPlayback applies from the next
    // Update on; starting at it avoids a first frame at the sample's old volume.
    sample->volume = Clamp01(volume);
    if (m_out == nullptr) return;
    if (sample->voice != 0) m_out->Stop(sample->voice);
    sample->voice = m_out->Play(sample->absolutePath, sample->loop, sample->volume * m_effectsVolume, sample->pan);
}

void SampleBank::ApplyEffect(const string& fileName, const bool stop, const bool loop, const float volume,
                             const float pan) {
    // HandleSoundPlayback (ETHParticleManager.cpp:729-779) acting on the SAME
    // sample object PlaySample uses: it overwrites the script's volume and pan.
    Sample* sample = Find(fileName);
    if (sample == nullptr) return;
    const float master = sample->music ? m_musicVolume : m_effectsVolume;
    if (stop) {
        if (m_out != nullptr && sample->voice != 0) m_out->Stop(sample->voice);
        sample->voice = 0;
        return;
    }
    sample->pan = ClampPan(pan);
    sample->volume = Clamp01(volume);
    const bool playing = m_out != nullptr && sample->voice != 0 && m_out->IsPlaying(sample->voice);
    if (playing) m_out->Set(sample->voice, sample->volume * master, sample->pan);
    // The loop rule (:761-772): not playing, or playing without its loop flag ->
    // Play() from the start and set the flag.
    if (loop && (!playing || !sample->loop)) {
        sample->loop = true;
        if (m_out == nullptr) return;
        if (sample->voice != 0) m_out->Stop(sample->voice);
        sample->voice = m_out->Play(sample->absolutePath, true, sample->volume * master, sample->pan);
    }
}

void SampleBank::ReleaseAll() {
    // ETHEngine::LoadScene -> ETHAudioResourceManager::ReleaseResources
    // (ETHEngine.cpp:826): every sample stops and is forgotten, with its volume,
    // pan and loop flag.
    if (m_out != nullptr) {
        for (auto& [ignoredKey, sample] : m_samples) {
            if (sample.voice != 0) m_out->Stop(sample.voice);
        }
        m_out->UnloadAll();
    }
    m_samples.clear();
}

} // namespace Penumbra::Eth
