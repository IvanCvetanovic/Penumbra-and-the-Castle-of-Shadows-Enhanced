#pragma once

// THE SCRIPT API: everything the original's AngelScript could call, under the
// names it called them, acting on Machine::Current(). A ported script includes
// this one header and `using namespace Penumbra::Eth;`, and its lines read like
// the lines they came from:
//
//   AngelScript                              C++
//   ETHEntity @e;                            ETHEntity e;
//   if (e !is null)                          if (e != nullptr)
//   e.AddUIntData("x", 1);                   e->AddUIntData("x", 1);
//   "lv" + g_charLevel[p]                    "lv" + Str(g_charLevel[p])
//   const uint t = GetTime()-start;          const uint t = GetTime()-start;   (uint32 wrap, same)
//   GetInputHandle().KeyDown(K_UP)           GetInputHandle().KeyDown(K_UP)
//   AddEntity("x.ent", p, @h);               AddEntity("x.ent", p, h);
//   dictionary d; d.get(k, @h)               dictionary<T> d; d.get(k, h)
//
// Semantics are 0.7.12's: docs/spec/30-ethanon-runtime.md §5 is the table.

#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "eth/EthTypes.hpp"
#include "eth/Entity.hpp"
#include "eth/Machine.hpp"

namespace Penumbra::Eth {

// --- Scenes -----------------------------------------------------------------
void LoadScene(const string& file);
void LoadScene(const string& file, const string& onLoad, const string& onLoop);
void LoadScene(const string& file, const string& onLoad, const string& onLoop, const vector2& bucketSize);
bool SaveScene(const string& file);
string GetSceneFileName();

// --- Entities ---------------------------------------------------------------
int AddEntity(const string& file, const vector3& pos, float angle);
int AddEntity(const string& file, const vector3& pos, ETHEntity& out);
int AddEntity(const string& file, const vector3& pos, const string& alternativeName);
ETHEntity DeleteEntity(ETHEntity entity);
ETHEntity SeekEntity(int id);
ETHEntity SeekEntity(const string& name);
bool GetEntityArray(const string& name, ETHEntityArray& out);
bool GetEntitiesFromBucket(const vector2& bucket, ETHEntityArray& out);
void GetVisibleEntities(ETHEntityArray& out);
bool Collide(const ETHEntity& entity);
bool Collide(const ETHEntity& entity, ETHEntity& out);
bool CollideStatic(const ETHEntity& entity);
bool CollideStatic(const ETHEntity& entity, ETHEntity& out);
bool CollideDynamic(const ETHEntity& entity);
bool CollideDynamic(const ETHEntity& entity, ETHEntity& out);
uint GetNumEntities();
int GetLastID();
vector2 GetBucket(const vector2& pos);

// --- Time ---------------------------------------------------------------------
uint GetTime();                     // simulated ms since boot, uint32, never reset
float GetTimeF();
float UnitsPerSecond(float value);  // value/60, and 0 on the frame of a scene load
float GetFPSRate();                 // 60

// --- Camera, screen, scene look --------------------------------------------------
void SetCameraPos(const vector2& pos);      // floored when round-up is on
void AddToCameraPos(const vector2& v);      // never floored
vector2 GetCameraPos();
void SetPositionRoundUp(bool roundUp);
bool GetPositionRoundUp();
void SetBorderBucketsDrawing(bool draw);
bool IsDrawingBorderBuckets();
vector2 GetScreenSize();
uint GetScreenWidth();
uint GetScreenHeight();
void SetAmbientLight(const vector3& color);
vector3 GetAmbientLight();
void SetBackgroundColor(uint color);
uint GetBackgroundColor();
bool SetBackgroundImage(const string& path);
void PositionBackgroundImage(const vector2& min, const vector2& max);
void SetBackgroundAlphaAdd();
void SetBackgroundAlphaPixel();
void UsePixelShaders(bool use);
bool IsPixelShaderSupported();

// --- Top layer (HUD) --------------------------------------------------------------
void DrawText(const vector2& pos, const string& text, const string& font, float size, uint color);
void LoadSprite(const string& path);
void DrawSprite(const string& path, const vector2& pos, uint color);
void DrawShapedSprite(const string& path, const vector2& pos, const vector2& size, uint color);
vector2 GetSpriteSize(const string& path);
void DrawRectangle(const vector2& pos, const vector2& size, uint c0, uint c1, uint c2, uint c3);

// --- Window -------------------------------------------------------------------------
void SetWindowProperties(const string& title, uint width, uint height, bool windowed, bool sync,
                         PIXEL_FORMAT format);
bool Windowed();
uint GetVideoModeCount();
videoMode GetVideoMode(uint index);
void HideCursor(bool hide);
void Exit();

// --- Audio ----------------------------------------------------------------------------
bool LoadMusic(const string& path);
bool LoadSoundEffect(const string& path);
bool PlaySample(const string& path);
bool LoopSample(const string& path, bool loop);
bool StopSample(const string& path);
bool PauseSample(const string& path);
bool SetSampleVolume(const string& path, float volume);
bool SetSamplePan(const string& path, float pan);
bool SampleExists(const string& path);
bool IsSamplePlaying(const string& path);

// --- Input ------------------------------------------------------------------------------
// GetInputHandle() returns the machine's InputState; `@input` handles in the
// scripts become `InputState& input = GetInputHandle();`.
InputState& GetInputHandle();

// --- Files ------------------------------------------------------------------------------
// GetAbsolutePath resolves a game-relative path. Reads prefer a copy the game
// has written to the user directory; writes of a game path land in the user
// directory - so hs.enml and scenes/checkpoint.esc work without the original's
// folder ever being written.
string GetAbsolutePath(const string& relative);
string GetStringFromFile(const string& absolutePath);   // "" when missing
bool SaveStringToFile(const string& absolutePath, const string& contents);
string GetProgramPath();

// --- Numbers and strings ------------------------------------------------------------------
int rand(int max);                  // [0, max] inclusive
int rand(int min, int max);
float randF(float max);             // [0, max]
float randF(float min, float max);
vector2 normalize(const vector2& v);        // NaN for a zero vector, as the original's rsqrt
vector3 normalize(const vector3& v);
float radianToDegree(float radians);        // a/(2pi)*360
float degreeToRadian(float degrees);
// atan2(v.x, v.y) - the ARGUMENTS SWAPPED, as registered - mapped into [0, 2pi).
float GetAngle(const vector2& v);
inline float abs(float v) { return std::fabs(v); }
inline int abs(int v) { return v < 0 ? -v : v; }
inline float sqrt(float v) { return std::sqrt(v); }
inline float floor(float v) { return std::floor(v); }
inline float min(float a, float b) { return a < b ? a : b; }
inline float max(float a, float b) { return a > b ? a : b; }
inline int min(int a, int b) { return a < b ? a : b; }
inline int max(int a, int b) { return a > b ? a : b; }
inline uint min(uint a, uint b) { return a < b ? a : b; }
inline uint max(uint a, uint b) { return a > b ? a : b; }

// AngelScript's string + number: integers as decimal, floats with 6
// significant digits (%g), bools as "true"/"false".
string Str(int value);
string Str(uint value);
string Str(float value);
string Str(double value);
string Str(bool value);
string Str(const vector2& value);

// Logging (print) - cp1252 converted to UTF-8 for the log.
void print(const string& text);

// --- ENML (enml.h, 0.7.12) -------------------------------------------------------------------
// name { key = value; } sections. Values keep their inner CRLFs; \; and \\
// escapes; leading whitespace skipped, trailing kept; '/' comments outside
// values. getInt/getUint/getFloat use sscanf and LEAVE `out` UNWRITTEN when the
// key is missing (data.enml has no global.lv20 and addToExp depends on it).
class enmlEntity {
public:
    void clear() { m_attributes.clear(); m_order.clear(); }
    void add(const string& name, const string& value);
    string get(const string& name) const;
    const std::vector<string>& Order() const { return m_order; }
private:
    std::map<string, string> m_attributes;
    std::vector<string> m_order;
};

class enmlFile {
public:
    uint parseString(const string& text);          // returns the number of entities read
    bool parseFromFile(const string& absolutePath);
    bool exists(const string& entity) const;
    string get(const string& entity, const string& attribute) const;   // "" when missing
    bool getInt(const string& entity, const string& attribute, int& out) const;
    bool getUint(const string& entity, const string& attribute, uint& out) const;
    bool getFloat(const string& entity, const string& attribute, float& out) const;
    bool getDouble(const string& entity, const string& attribute, double& out) const;
    void addEntity(const string& name, const enmlEntity& entity);
    string generateString() const;
    // Writes generateString() to a game path, redirected to the user directory.
    void writeToFile(const string& absolutePath) const;
private:
    std::map<string, enmlEntity> m_entities;
    std::vector<string> m_order;
};

// --- dictionary (the AngelScript add-on), as the scripts use it: a map from a
// string key to a handle. get() returns false and leaves `out` alone when the
// key is missing.
template <typename T>
class dictionary {
public:
    void set(const string& key, std::shared_ptr<T> value) { m_values[key] = std::move(value); }
    bool get(const string& key, std::shared_ptr<T>& out) const {
        const auto it = m_values.find(key);
        if (it == m_values.end()) return false;
        out = it->second;
        return true;
    }
    bool exists(const string& key) const { return m_values.count(key) != 0; }
    void deleteKey(const string& key) { m_values.erase(key); }   // AngelScript's delete()
    void deleteAll() { m_values.clear(); }
private:
    std::map<string, std::shared_ptr<T>> m_values;
};

} // namespace Penumbra::Eth
