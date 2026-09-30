#pragma once

// THE ORIGINAL'S SOUNDS, DECODED ON EVERY PLATFORM.
//
// The original ships 19 Ogg Vorbis files and 16 MP3s: the menu's and every
// level's music, the boss's, and a dozen effects (docs/spec, test_pn_audio).
// The engine decodes Ogg everywhere (stb_vorbis) but MP3 only on Windows,
// through Media Foundation (engine/src/core/AudioClip.hpp) - off Windows
// every MP3 the scripts load would be silent. So the port decodes MP3 itself
// with dr_mp3 (game/third_party/dr_mp3, public domain / MIT-0) where the
// engine cannot:
//
//   Windows    the engine's decoder, as before; dr_mp3 only for an MP3 the
//              engine refuses (a file Media Foundation cannot decode);
//   elsewhere  dr_mp3 for every .mp3, the engine for everything else.
//
// The clip is the engine's AudioClip, 16-bit interleaved PCM, handed to
// AudioEngine::AddClip under the file's path (render/AudioOutEngine.cpp), so
// Play() finds it exactly as it finds a clip the engine loaded itself.

#include <string>

#include "core/AudioClip.hpp"

namespace Penumbra::Eth {

// Whether `path` names an MP3 (.mp3, any case).
bool IsMp3(const std::string& path);

// Whether the engine's own AudioClip::Load decodes `path` on this platform
// (everything but .mp3 off Windows).
bool EngineDecodes(const std::string& path);

// An MP3 file, decoded whole by dr_mp3. One or two channels, as the engine
// accepts.
bool DecodeMp3(const std::string& path, Supersonic::AudioClip& out, std::string& error);

// AudioClip::Load where the engine decodes the file, DecodeMp3 where it does
// not, and DecodeMp3 again for an MP3 the engine refused.
bool LoadSound(const std::string& path, Supersonic::AudioClip& out, std::string& error);

} // namespace Penumbra::Eth
