#include "render/AudioOutEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <system_error>
#include <utility>

#include "core/AudioClip.hpp"
#include "core/AudioEngine.hpp"
#include "core/Log.hpp"
#include "eth/SoundDecode.hpp"

namespace Penumbra::Render {

namespace {

using EngineVoice = Supersonic::AudioEngine::VoiceId;

// The engine's ids start at 1 and are never reused, so they pass through as
// they are; its kInvalidVoice is the bank's 0.
Eth::VoiceId FromEngine(EngineVoice voice) {
    return voice == Supersonic::AudioEngine::kInvalidVoice ? 0 : static_cast<Eth::VoiceId>(voice);
}

bool ToEngine(Eth::VoiceId voice, EngineVoice& out) {
    if (voice == 0 || voice >= Supersonic::AudioEngine::kInvalidVoice) return false;
    out = static_cast<EngineVoice>(voice);
    return true;
}

// Audiere's volume was linear and clamped to 0..1, as the engine's is.
float Volume(float volume) { return std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 0.0f; }
float Pan(float pan) { return std::isfinite(pan) ? std::clamp(pan, -1.0f, 1.0f) : 0.0f; }

constexpr float kPitch = 1.0f;   // the scripts never change pitch

} // namespace

void AudioOutEngine::Attach(entt::registry& registry) {
    Supersonic::AudioEngine** slot = registry.ctx().find<Supersonic::AudioEngine*>();
    Attach(slot != nullptr ? *slot : nullptr);
}

void AudioOutEngine::Attach(Supersonic::AudioEngine* engine) {
    if (engine == m_engine) return;
    Detach();
    m_engine = engine;
}

void AudioOutEngine::Detach() {
    if (m_engine != nullptr) {
        // Stop BEFORE unload: a voice reads the clip's samples from the audio
        // thread, so unloading under a playing voice frees memory it is
        // reading (AudioEngine::UnloadClip).
        for (const std::string& path : m_loaded) {
            m_engine->StopVoicesUsing(path);
            m_engine->UnloadClip(path);
        }
    }
    m_loaded.clear();
    m_engine = nullptr;
}

bool AudioOutEngine::Available() const { return m_engine != nullptr && m_engine->IsAvailable(); }

bool AudioOutEngine::Load(const Eth::string& absolutePath, bool music) {
    // Music is decoded whole like everything else: the engine has no streaming
    // voice (see the header).
    (void)music;
    if (!Available()) {
        // No device: nothing will ever play, so do not spend a second decoding
        // fase.mp3 - but answer as 0.7.12 would, by whether the file is there.
        std::error_code ec;
        return std::filesystem::is_regular_file(std::filesystem::path(absolutePath), ec);
    }
    // Recorded even when the decode fails: UnloadClip also clears the engine's
    // cached failure, so the next scene load tries again.
    m_loaded.insert(absolutePath);
    return ensureClip(absolutePath);
}

bool AudioOutEngine::ensureClip(const std::string& absolutePath) {
    if (m_engine->HasClip(absolutePath)) return true;
    if (Eth::EngineDecodes(absolutePath)) {
        if (m_engine->LoadClip(absolutePath) != nullptr) return true;
        if (!Eth::IsMp3(absolutePath)) return false;   // the engine's verdict, logged by it
    }
    // An MP3 the engine cannot decode (off Windows) or would not (a Windows
    // without Media Foundation): decoded here and handed over under the same
    // name, which also replaces a failure the engine cached for it.
    Supersonic::AudioClip clip;
    std::string error;
    if (!Eth::DecodeMp3(absolutePath, clip, error)) {
        SUPERSONIC_LOG_ERROR("Penumbra") << error << std::endl;
        return false;
    }
    // Said as the engine's LoadClip says it, since AddClip itself is silent:
    // without this line a decoded MP3 and one never asked for look the same.
    SUPERSONIC_LOG_INFO("Penumbra") << "Decoded " << absolutePath << " (MP3, " << clip.channels << "ch, "
                                    << clip.sampleRate << " Hz, " << clip.durationSeconds() << "s)." << std::endl;
    return m_engine->AddClip(absolutePath, std::move(clip)) != nullptr;
}

Eth::VoiceId AudioOutEngine::Play(const Eth::string& absolutePath, bool loop, float volume, float pan) {
    if (!Available()) return 0;
    // Play loads on demand; remember the path so UnloadAll releases it. Loaded
    // here first, because the engine's own on-demand load cannot decode what
    // ensureClip decodes for it.
    m_loaded.insert(absolutePath);
    ensureClip(absolutePath);
    const EngineVoice voice = m_engine->Play(absolutePath, loop, Volume(volume), kPitch);
    if (voice == Supersonic::AudioEngine::kInvalidVoice) return 0;
    m_engine->SetVoiceParameters(voice, Volume(volume), kPitch, Pan(pan));
    return FromEngine(voice);
}

void AudioOutEngine::Stop(Eth::VoiceId voice) {
    EngineVoice engineVoice = 0;
    if (m_engine == nullptr || !ToEngine(voice, engineVoice)) return;
    m_engine->Stop(engineVoice);
}

void AudioOutEngine::Set(Eth::VoiceId voice, float volume, float pan) {
    EngineVoice engineVoice = 0;
    if (m_engine == nullptr || !ToEngine(voice, engineVoice)) return;
    m_engine->SetVoiceParameters(engineVoice, Volume(volume), kPitch, Pan(pan));
}

bool AudioOutEngine::IsPlaying(Eth::VoiceId voice) {
    EngineVoice engineVoice = 0;
    if (m_engine == nullptr || !ToEngine(voice, engineVoice)) return false;
    // A voice that finished by itself has been reaped by AudioSystem::Update
    // (ReapFinishedVoices) and reads as not playing, which is the answer.
    return m_engine->IsVoicePlaying(engineVoice);
}

void AudioOutEngine::UnloadAll() {
    if (m_engine == nullptr) {
        m_loaded.clear();
        return;
    }
    for (const std::string& path : m_loaded) {
        m_engine->StopVoicesUsing(path);
        if (!m_keepDecoded) m_engine->UnloadClip(path);
    }
    // Kept clips stay listed so Detach still releases them.
    if (!m_keepDecoded) m_loaded.clear();
}

} // namespace Penumbra::Render
