#include "render/AudioOutEngine.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <system_error>
#include <utility>

#include "core/AudioClip.hpp"
#include "core/AudioEngine.hpp"
#include "core/Log.hpp"
#include "eth/AudioPrefetch.hpp"
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

AudioOutEngine::AudioOutEngine() = default;

// Joins the worker (it finishes the file it is on first): this is why the destructor is out of line.
AudioOutEngine::~AudioOutEngine() = default;

void AudioOutEngine::startPrefetchBeside(const std::string& absolutePath) {
    if (m_prefetchStarted) return;
    m_prefetchStarted = true;
#ifdef _WIN32
    // Windows' MP3 decoder is the engine's (Media Foundation), fast and not meant for a thread of its own.
    (void)absolutePath;
#else
    // Only the game's own sounds: a folder named soundfx (the original's), in whatever directory the data was unpacked to.
    std::error_code ec;
    const std::filesystem::path file(absolutePath);
    std::string folder = file.parent_path().filename().string();
    std::transform(folder.begin(), folder.end(), folder.begin(), [](unsigned char c) { return std::tolower(c); });
    if (folder != "soundfx") return;

    struct Item {
        std::string path;
        std::uintmax_t size;
    };
    std::vector<Item> items;
    for (const auto& entry : std::filesystem::directory_iterator(file.parent_path(), ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path() == file) continue;   // being decoded by the caller, now
        const std::string path = entry.path().string();
        // What the engine or the port decodes: the extensions the scripts ask for.
        if (!Eth::IsMp3(path)) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
            if (ext != ".ogg") continue;
        }
        items.push_back({path, entry.file_size(ec)});
    }
    // The longest first (the level's two pieces of music): they are the ones whose decode a late New Game would wait for.
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.size != b.size ? a.size > b.size : a.path < b.path; });
    std::vector<std::string> paths;
    for (const Item& item : items) paths.push_back(item.path);
    if (paths.empty()) return;

    m_prefetch = std::make_unique<Eth::AudioPrefetch>(
        [](const std::string& path, Supersonic::AudioClip& out, std::string& error) { return Eth::LoadSound(path, out, error); });
    m_prefetch->Request(paths);
    SUPERSONIC_LOG_INFO("Penumbra") << "Decoding " << paths.size() << " more sounds from " << file.parent_path().string()
                                    << " in the background." << std::endl;
#endif
}

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
    startPrefetchBeside(absolutePath);
    if (m_prefetch) {
        // Decoded by the worker already (moved out at once), or being decoded now (waited for: at most that one file's remainder).
        // Not there, not started or failed: the caller's own decode below, as ever.
        Supersonic::AudioClip prefetched;
        std::string ignored;
        if (m_prefetch->Take(absolutePath, prefetched, ignored)) {
            SUPERSONIC_LOG_INFO("Penumbra") << "Decoded " << absolutePath << " (in the background, " << prefetched.channels << "ch, "
                                            << prefetched.sampleRate << " Hz, " << prefetched.durationSeconds() << "s)." << std::endl;
            if (m_engine->AddClip(absolutePath, std::move(prefetched)) != nullptr) return true;
        }
    }
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
