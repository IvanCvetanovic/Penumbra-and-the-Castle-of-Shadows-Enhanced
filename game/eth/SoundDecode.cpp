#include "eth/SoundDecode.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "third_party/dr_mp3/dr_mp3.h"

namespace Penumbra::Eth {

namespace {

struct DrMp3Free {
    void operator()(drmp3_int16* samples) const { drmp3_free(samples, nullptr); }
};

} // namespace

bool IsMp3(const std::string& path) {
    const std::string ext = std::filesystem::path(path).extension().string();
    return ext.size() == 4 && ext[0] == '.' && (ext[1] == 'm' || ext[1] == 'M') && (ext[2] == 'p' || ext[2] == 'P') &&
           ext[3] == '3';
}

bool EngineDecodes(const std::string& path) {
#ifdef _WIN32
    (void)path;
    return true;
#else
    return !IsMp3(path);
#endif
}

bool DecodeMp3(const std::string& path, Supersonic::AudioClip& out, std::string& error) {
    out = Supersonic::AudioClip{};
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.empty()) {
        error = path + " is empty";
        return false;
    }

    drmp3_config config{};
    drmp3_uint64 frames = 0;
    const std::unique_ptr<drmp3_int16, DrMp3Free> samples(
        drmp3_open_memory_and_read_pcm_frames_s16(bytes.data(), bytes.size(), &config, &frames, nullptr));
    if (samples == nullptr || frames == 0) {
        error = "cannot decode " + path + " as an MP3";
        return false;
    }
    // The limits the engine's own decoders state: the backend computes its
    // block alignment from the channel count.
    if (config.channels < 1 || config.channels > 2 || config.sampleRate == 0) {
        error = path + " has unsupported channel count " + std::to_string(config.channels);
        return false;
    }
    const std::uint64_t count = static_cast<std::uint64_t>(frames) * config.channels;
    if (count > std::numeric_limits<std::size_t>::max() / 2) {
        error = path + " is too long to decode in one piece";
        return false;
    }

    // Little-endian bytes, the clip's contract whatever the host's order.
    std::vector<std::uint8_t> pcm(static_cast<std::size_t>(count) * 2);
    for (std::size_t i = 0; i < static_cast<std::size_t>(count); ++i) {
        const auto sample = static_cast<std::uint16_t>(samples.get()[i]);
        pcm[2 * i] = static_cast<std::uint8_t>(sample & 0xFFu);
        pcm[2 * i + 1] = static_cast<std::uint8_t>(sample >> 8);
    }
    out.channels = static_cast<std::uint16_t>(config.channels);
    out.sampleRate = config.sampleRate;
    out.bitsPerSample = 16;
    out.pcm = std::move(pcm);
    return true;
}

bool LoadSound(const std::string& path, Supersonic::AudioClip& out, std::string& error) {
    if (!EngineDecodes(path)) return DecodeMp3(path, out, error);
    if (Supersonic::AudioClip::Load(path, out, error)) return true;
    if (!IsMp3(path)) return false;
    std::string ours;
    if (DecodeMp3(path, out, ours)) return true;
    error += "; " + ours;
    return false;
}

} // namespace Penumbra::Eth
