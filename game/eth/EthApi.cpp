// The script API's free functions (eth/Eth.hpp), acting on Machine::Current(),
// as ETHEngine's static wrappers acted on its one engine
// (reference/eth-0.7.12/src/ETHEngine.cpp:267-436 registers them).
// Str/print live in Text.cpp and the ENML classes in Enml.cpp.

#include "eth/Eth.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#include "core/Log.hpp"

namespace Penumbra::Eth {

namespace {

Machine& M() { return Machine::Current(); }

// A path inside the original's folder (extracted/app), slashes of either kind,
// case-insensitive: nothing is ever written there.
bool InsideOriginal(const string& path) {
    string p = path;
    string r = PENUMBRA_ORIGINAL_DIR;
    std::replace(p.begin(), p.end(), '\\', '/');
    std::replace(r.begin(), r.end(), '\\', '/');
    if (p.size() <= r.size() || p[r.size()] != '/') return false;
    for (std::size_t i = 0; i < r.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(p[i])) != std::tolower(static_cast<unsigned char>(r[i]))) {
            return false;
        }
    }
    return true;
}

// GS_PI as GameSpaceLib defined it.
constexpr float kPi = 3.14159265358979323846f;

} // namespace

// --- Scenes -----------------------------------------------------------------

void LoadScene(const string& file) { M().LoadScene(file); }

void LoadScene(const string& file, const string& onLoad, const string& onLoop) { M().LoadScene(file, onLoad, onLoop); }

void LoadScene(const string& file, const string& onLoad, const string& onLoop, const vector2& bucketSize) {
    M().LoadScene(file, onLoad, onLoop, bucketSize);
}

bool SaveScene(const string& file) { return M().SaveScene(file); }

string GetSceneFileName() { return M().GetSceneFileName(); }

// --- Entities ---------------------------------------------------------------

int AddEntity(const string& file, const vector3& pos, const float angle) { return M().AddEntity(file, pos, angle); }

int AddEntity(const string& file, const vector3& pos, ETHEntity& out) { return M().AddEntity(file, pos, out); }

int AddEntity(const string& file, const vector3& pos, const string& alternativeName) {
    return M().AddEntity(file, pos, alternativeName);
}

ETHEntity DeleteEntity(ETHEntity entity) { return M().DeleteEntity(std::move(entity)); }

ETHEntity SeekEntity(const int id) { return M().SeekEntity(id); }

ETHEntity SeekEntity(const string& name) { return M().SeekEntity(name); }

bool GetEntityArray(const string& name, ETHEntityArray& out) { return M().GetEntityArray(name, out); }

bool GetEntitiesFromBucket(const vector2& bucket, ETHEntityArray& out) { return M().GetEntitiesFromBucket(bucket, out); }

void GetVisibleEntities(ETHEntityArray& out) { M().GetVisibleEntities(out); }

// The @&out forms: AngelScript hands the function a null temporary and copies
// it back, so a miss leaves the variable null.
bool Collide(const ETHEntity& entity) { return M().Collide(entity, 0, nullptr); }

bool Collide(const ETHEntity& entity, ETHEntity& out) {
    ETHEntity hit;
    const bool r = M().Collide(entity, 0, &hit);
    out = hit;
    return r;
}

bool CollideStatic(const ETHEntity& entity) { return M().Collide(entity, 1, nullptr); }

bool CollideStatic(const ETHEntity& entity, ETHEntity& out) {
    ETHEntity hit;
    const bool r = M().Collide(entity, 1, &hit);
    out = hit;
    return r;
}

bool CollideDynamic(const ETHEntity& entity) { return M().Collide(entity, 2, nullptr); }

bool CollideDynamic(const ETHEntity& entity, ETHEntity& out) {
    ETHEntity hit;
    const bool r = M().Collide(entity, 2, &hit);
    out = hit;
    return r;
}

uint GetNumEntities() { return M().GetNumEntities(); }

int GetLastID() { return M().GetLastID(); }

vector2 GetBucket(const vector2& pos) {
    // ETHEngine.cpp:1507-1510 with the current scene's bucket size.
    const Scene* scene = M().CurrentScene();
    const vector2 size = scene != nullptr ? scene->BucketSize() : vector2(256.0f);
    return vector2(std::floor(pos.x / size.x), std::floor(pos.y / size.y));
}

// --- Time -------------------------------------------------------------------------

uint GetTime() { return M().GetTime(); }

float GetTimeF() { return M().GetTimeF(); }

float UnitsPerSecond(const float value) { return M().UnitsPerSecond(value); }

float GetFPSRate() { return M().GetFPSRate(); }

// --- Camera, screen, scene look --------------------------------------------------------

void SetCameraPos(const vector2& pos) { M().SetCameraPos(pos); }

void AddToCameraPos(const vector2& v) { M().AddToCameraPos(v); }

vector2 GetCameraPos() { return M().GetCameraPos(); }

void SetPositionRoundUp(const bool roundUp) { M().SetPositionRoundUp(roundUp); }

bool GetPositionRoundUp() { return M().GetPositionRoundUp(); }

void SetBorderBucketsDrawing(const bool draw) { M().SetBorderBucketsDrawing(draw); }

bool IsDrawingBorderBuckets() { return M().IsDrawingBorderBuckets(); }

vector2 GetScreenSize() { return M().GetScreenSize(); }

// 0.7.12 answered these from the back buffer it had been asked for; the port's
// logical screen is that back buffer.
uint GetScreenWidth() { return static_cast<uint>(M().GetScreenSize().x); }

uint GetScreenHeight() { return static_cast<uint>(M().GetScreenSize().y); }

void SetAmbientLight(const vector3& color) { M().SetAmbientLight(color); }

vector3 GetAmbientLight() { return M().GetAmbientLight(); }

void SetBackgroundColor(const uint color) { M().SetBackgroundColor(color); }

uint GetBackgroundColor() { return M().GetBackgroundColor(); }

bool SetBackgroundImage(const string& path) { return M().SetBackgroundImage(path); }

void PositionBackgroundImage(const vector2& min, const vector2& max) { M().PositionBackgroundImage(min, max); }

void SetBackgroundAlphaAdd() { M().SetBackgroundAlphaAdd(); }

void SetBackgroundAlphaPixel() { M().SetBackgroundAlphaPixel(); }

void UsePixelShaders(const bool use) { M().UsePixelShaders(use); }

bool IsPixelShaderSupported() { return M().IsPixelShaderSupported(); }

// --- Top layer (HUD) -----------------------------------------------------------------------

void DrawText(const vector2& pos, const string& text, const string& font, const float size, const uint color) {
    M().DrawText(pos, text, font, size, color);
}

void DrawText(const vector2& pos, const string& text, const string& font, const float size, const uint color,
              const float rtlRight) {
    M().DrawText(pos, text, font, size, color, rtlRight);
}

void DrawText(const vector2& pos, const string& text, const string& font, const float size, const uint color,
              const float rtlRight, const TextFit& fit) {
    M().DrawText(pos, text, font, size, color, rtlRight, fit);
}

void LoadSprite(const string& path) { M().LoadSprite(path); }

void DrawSprite(const string& path, const vector2& pos, const uint color) { M().DrawSprite(path, pos, color); }

void DrawShapedSprite(const string& path, const vector2& pos, const vector2& size, const uint color) {
    M().DrawShapedSprite(path, pos, size, color);
}

void DrawSpritePart(const string& path, const vector2& pos, const vector2& rectMin, const vector2& rectMax,
                    const uint color) {
    M().DrawSpritePart(path, pos, rectMin, rectMax, color);
}

void DrawShapedSpritePart(const string& path, const vector2& pos, const vector2& size, const vector2& rectMin,
                          const vector2& rectMax, const uint color) {
    M().DrawShapedSpritePart(path, pos, size, rectMin, rectMax, color);
}

vector2 GetSpriteSize(const string& path) { return M().GetSpriteSize(path); }

void DrawRectangle(const vector2& pos, const vector2& size, const uint c0, const uint c1, const uint c2,
                   const uint c3) {
    M().DrawRectangle(pos, size, c0, c1, c2, c3);
}

// --- Window --------------------------------------------------------------------------------

void SetWindowProperties(const string& title, const uint width, const uint height, const bool windowed,
                         const bool sync, const PIXEL_FORMAT format) {
    M().SetWindowProperties(title, width, height, windowed, sync, format);
}

bool Windowed() { return M().Windowed(); }

uint GetVideoModeCount() { return M().GetVideoModeCount(); }

videoMode GetVideoMode(const uint index) { return M().GetVideoMode(index); }

void HideCursor(const bool hide) { M().HideCursor(hide); }

void Exit() { M().Exit(); }

// --- Audio ---------------------------------------------------------------------------------

bool LoadMusic(const string& path) { return M().Samples().LoadMusic(path); }

bool LoadSoundEffect(const string& path) { return M().Samples().LoadSoundEffect(path); }

bool PlaySample(const string& path) { return M().Samples().PlaySample(path); }

bool LoopSample(const string& path, const bool loop) { return M().Samples().LoopSample(path, loop); }

bool StopSample(const string& path) { return M().Samples().StopSample(path); }

bool PauseSample(const string& path) { return M().Samples().PauseSample(path); }

bool SetSampleVolume(const string& path, const float volume) { return M().Samples().SetSampleVolume(path, volume); }

bool SetSamplePan(const string& path, const float pan) { return M().Samples().SetSamplePan(path, pan); }

bool SampleExists(const string& path) { return M().Samples().SampleExists(path); }

bool IsSamplePlaying(const string& path) { return M().Samples().IsSamplePlaying(path); }

bool KeepSampleOnNextLoad(const string& path) { return M().Samples().KeepOnNextLoad(path); }   // E30

// --- Input ---------------------------------------------------------------------------------

InputState& GetInputHandle() { return M().Input(); }

// --- Files ---------------------------------------------------------------------------------

string GetAbsolutePath(const string& relative) { return M().GetAbsolutePath(relative); }

string GetStringFromFile(const string& absolutePath) {
    // enml::getStringFromFileC (enml.h:107-125): a TEXT-mode ifstream read
    // char by char, so CRLF arrives as LF, NULs are dropped and MSVC's text
    // mode ends the file at a Ctrl-Z. A game path reads the user copy first.
    const string path = M().ReadPath(absolutePath);
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) return string();
    const string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    string text;
    text.reserve(bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const char c = bytes[i];
        if (c == '\x1a') break;
        if (c == '\0') continue;
        if (c == '\r' && i + 1 < bytes.size() && bytes[i + 1] == '\n') continue;
        text += c;
    }
    return text;
}

bool SaveStringToFile(const string& absolutePath, const string& contents) {
    // enml::saveStringToFileC (enml.h:135-145), a text-mode ofstream: every
    // '\n' reaches the disk as CRLF. A game path is redirected to the user
    // directory; nothing is written into the original's folder.
    string target = absolutePath;
    if (M().IsGamePath(absolutePath)) {
        target = M().WritePath(absolutePath);
        if (target.empty()) {
            SUPERSONIC_LOG_WARN("Penumbra") << "SaveStringToFile: no user directory, " << absolutePath
                                            << " not written";
            return false;
        }
    }
    if (InsideOriginal(target)) {
        SUPERSONIC_LOG_ERROR("Penumbra") << "SaveStringToFile: refusing to write into the original's folder: "
                                         << target;
        return false;
    }
    std::ofstream file(std::filesystem::path(target), std::ios::binary | std::ios::trunc);
    if (!file) return false;
    string bytes;
    bytes.reserve(contents.size() + contents.size() / 8);
    for (const char c : contents) {
        if (c == '\n') bytes += '\r';
        bytes += c;
    }
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(file);
}

string GetProgramPath() { return M().Config().gameRoot; }

// --- Numbers ---------------------------------------------------------------------------------

int rand(const int max) { return M().Rng().RandI(max); }

int rand(const int min, const int max) { return M().Rng().RandI(min, max); }

float randF(const float max) { return M().Rng().RandF(max); }

float randF(const float min, const float max) { return M().Rng().RandF(min, max); }

vector2 normalize(const vector2& v) {
    // ETHGlobal::normalize2 (ETHCommon.h:397-402): v * rsqrt(v.v), so a zero
    // vector is NaN, not zero.
    const float a = 1.0f / std::sqrt(v.x * v.x + v.y * v.y);
    return vector2(v.x * a, v.y * a);
}

vector3 normalize(const vector3& v) {
    const float a = 1.0f / std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return vector3(v.x * a, v.y * a, v.z * a);
}

float radianToDegree(const float radians) { return (radians / (kPi * 2.0f)) * 360.0f; }

float degreeToRadian(const float degrees) { return degrees * (kPi / 180.0f); }

float GetAngle(const vector2& v) {
    // ETHScriptObjRegister.cpp:105-109: atan2(x, y), the arguments swapped.
    const float r = std::atan2(v.x, v.y);
    return r < 0.0f ? r + 2.0f * kPi : r;
}

} // namespace Penumbra::Eth
