#pragma once

// The Ethanon 0.7.12 "machine": everything the original's scripts could reach,
// and the frame that ran them.
//
// ONE Frame() IS ONE ETHANON FRAME, run on one 60 Hz engine tick, in 0.7.12's
// exact order (ETHEngine.cpp:565-623, docs/spec/30-ethanon-runtime.md §1):
//
//   1. input:       every key/button state machine steps once
//   2. loop:        the scene's loop function (e.g. "levelLoop")
//   3. load:        a pending LoadScene - requested by main(), by that loop,
//                   or by the PREVIOUS frame's callbacks - runs now: every
//                   sample and sprite released, the new scene read, the camera
//                   reset to (0,0), the background image cleared, then the new
//                   scene's preLoop runs and its loop is installed
//   4. timer:       UnitsPerSecond's frame length (0 right after a load)
//   5. render:      temporary entities' particles advance and finished ones are
//                   deleted; visible buckets are collected; the snapshot is
//                   taken (sprites in drawHash order, lights, particles, and the
//                   HUD primitives queued since the last render); static
//                   entities with callbacks that were drawn are listed
//   6. callbacks:   every dynamic-or-temporary entity's ETHCallback_*, in
//                   insertion order, visible or not; then the static ones listed
//                   in 5, in draw order. HUD primitives they queue are drawn in
//                   the NEXT frame's render, ahead of that frame's loop's.
//
// Script code runs inside try/catch(ScriptException): a null handle or bad
// index aborts only the function that was running, as in AngelScript.
//
// Machine::Current() is the machine the free functions in Eth.hpp act on. There
// is one per game (and one per suite); Scope makes one current.

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "eth/Audio.hpp"
#include "eth/Defs.hpp"
#include "eth/Entity.hpp"
#include "eth/Input.hpp"
#include "eth/Random.hpp"
#include "eth/Scene.hpp"
#include "eth/Snapshot.hpp"

namespace Penumbra::Eth {

using ScriptFunction = std::function<void()>;
using CallbackFunction = std::function<void(ETHEntity)>;

struct MachineConfig {
    // The original's root: extracted/app. Read-only.
    string gameRoot = PENUMBRA_ORIGINAL_DIR;
    // Where the game may write (hs.enml, scenes/checkpoint.esc, settings). Empty
    // = nothing is written (a suite, or a machine with no user directory).
    string userRoot;
    // The logical screen the scripts see (GetScreenSize). 1024x768 is the
    // original; the enhanced widescreen view is wider. May change per scene:
    // see ScreenSizeForScene.
    vector2 screenSize{1024.0f, 768.0f};
    std::function<vector2(const string& sceneFile)> screenSizeForScene;
    std::uint32_t seed = 5489u;
};

// What the scripts asked of the window (SetWindowProperties, HideCursor, Exit).
struct WindowRequest {
    string title;
    uint width = 1024;
    uint height = 768;
    bool windowed = true;
    bool changed = false;       // set when the scripts called SetWindowProperties since the last take
};

class Machine {
public:
    explicit Machine(MachineConfig config);
    ~Machine();

    Machine(const Machine&) = delete;
    Machine& operator=(const Machine&) = delete;

    static Machine& Current();
    static Machine* CurrentOrNull();
    // Makes a machine current for a scope (the layer's tick, a suite's body).
    class Scope {
    public:
        explicit Scope(Machine& machine);
        ~Scope();
    private:
        Machine* m_previous;
    };

    // --- Script registration (the ported scripts register themselves) ------
    void RegisterFunction(const string& name, ScriptFunction fn);     // loop / preLoop targets of LoadScene
    void RegisterCallback(const string& name, CallbackFunction fn);   // "ETHCallback_<name>" -> fn
    bool HasCallback(const string& name) const;

    // Runs the script's main() - before any frame, as 0.7.12 did before the
    // window existed. LoadScene requests made there are served by frame 1.
    void Boot(const ScriptFunction& main);

    // One Ethanon frame (see the header comment).
    void Frame(const InputFrame& input);

    const RenderSnapshot& Snapshot() const { return m_snapshot; }
    // Frames run so far; GetTime() is FrameIndex()*1000/60 ms, exactly.
    uint FrameIndex() const { return m_frameIndex; }
    // How many script calls a ScriptException aborted (each one logged once
    // per site): the suites check it, the layer may show it.
    uint ScriptAborts() const { return m_scriptAborts; }
    // Each distinct "<where>: <what>" that aborted, as logged (once per site).
    const std::set<string>& AbortSites() const { return m_loggedAborts; }

    // --- The runtime API (the free functions in Eth.hpp forward here) ------
    uint GetTime() const { return m_timeMs; }
    float GetTimeF() const { return static_cast<float>(m_timeMs); }
    float UnitsPerSecond(float value) const { return value * m_frameSeconds; }
    float GetFPSRate() const { return 60.0f; }

    // The three registered LoadScene forms (ETHEngine.cpp:853-883). They only
    // record the request; Frame() serves it after the next loop. The 1-argument
    // form clears preLoop and loop - and an empty loop name leaves the current
    // loop running (LoadSceneScripts, ETHEngine.cpp:885-912). The 3-argument
    // form keeps a bucket size an earlier request of the same frame gave.
    void LoadScene(const string& file);
    void LoadScene(const string& file, const string& onLoad, const string& onLoop);
    void LoadScene(const string& file, const string& onLoad, const string& onLoop, const vector2& bucketSize);
    bool SaveScene(const string& file);
    string GetSceneFileName() const;

    int AddEntity(const string& file, const vector3& pos, float angle);
    int AddEntity(const string& file, const vector3& pos, ETHEntity& out);
    int AddEntity(const string& file, const vector3& pos, const string& alternativeName);
    ETHEntity DeleteEntity(ETHEntity entity);
    ETHEntity SeekEntity(int id) const;
    ETHEntity SeekEntity(const string& name) const;
    bool GetEntityArray(const string& name, ETHEntityArray& out) const;
    bool GetEntitiesFromBucket(const vector2& bucket, ETHEntityArray& out) const;
    void GetVisibleEntities(ETHEntityArray& out) const;
    bool Collide(const ETHEntity& entity, int which, ETHEntity* out) const;
    uint GetNumEntities() const;
    int GetLastID() const;

    void SetCameraPos(const vector2& pos);
    void AddToCameraPos(const vector2& v);
    vector2 GetCameraPos() const { return m_camera; }
    void SetPositionRoundUp(bool roundUp) { m_roundUp = roundUp; }
    bool GetPositionRoundUp() const { return m_roundUp; }
    void SetBorderBucketsDrawing(bool draw);
    bool IsDrawingBorderBuckets() const;
    vector2 GetScreenSize() const { return m_screenSize; }

    void SetAmbientLight(const vector3& color);
    vector3 GetAmbientLight() const;
    void SetBackgroundColor(uint color) { m_backgroundColor = color; }
    uint GetBackgroundColor() const { return m_backgroundColor; }
    bool SetBackgroundImage(const string& path);
    void PositionBackgroundImage(const vector2& min, const vector2& max);
    void SetBackgroundAlphaAdd() { m_backgroundAdditive = true; }
    void SetBackgroundAlphaPixel() { m_backgroundAdditive = false; }
    void UsePixelShaders(bool use) { m_pixelShaders = use; }
    bool IsPixelShaderSupported() const { return true; }

    void DrawText(const vector2& pos, const string& text, const string& font, float size, uint color);
    void LoadSprite(const string& path);
    void DrawSprite(const string& path, const vector2& pos, uint color);
    void DrawShapedSprite(const string& path, const vector2& pos, const vector2& size, uint color);
    vector2 GetSpriteSize(const string& path) const;
    void DrawRectangle(const vector2& pos, const vector2& size, uint c0, uint c1, uint c2, uint c3);

    void SetWindowProperties(const string& title, uint width, uint height, bool windowed, bool sync,
                             PIXEL_FORMAT format);
    bool Windowed() const { return m_window.windowed; }
    uint GetVideoModeCount() const;
    videoMode GetVideoMode(uint index) const;
    void SetVideoModes(std::vector<videoMode> modes) { m_videoModes = std::move(modes); }
    void HideCursor(bool hide) { m_cursorHidden = hide; }
    void Exit() { m_quit = true; }

    InputState& Input() { return m_input; }
    SampleBank& Samples() { return m_samples; }
    Random& Rng() { return m_rng; }

    // Files. GetAbsolutePath resolves under the game root (programPath + "/" +
    // f, ETHEngine.cpp:1616-1619); the scripts' two writes (hs.enml,
    // scenes/checkpoint.esc) are redirected to the user root.
    //
    // ReadPath and WritePath take EITHER a path relative to the game root
    // ("scenes/checkpoint.esc") OR an absolute path under the game root (what
    // GetAbsolutePath returned, e.g. in enmlFile::writeToFile); both name the
    // same file. ReadPath: the user root's copy if it exists, else the
    // original's; a path outside the game root comes back unchanged.
    // WritePath: the same relative path under the user root, with its parent
    // directories created; "" when there is no user root or the path is not
    // under the game root (nothing is ever written into the original).
    string GetAbsolutePath(const string& relative) const;
    string ReadPath(const string& relative) const;
    string WritePath(const string& relative) const;
    // A relative path, or an absolute one under the game root: a path the two
    // above redirect.
    bool IsGamePath(const string& path) const;

    // --- For the layer ------------------------------------------------------
    bool QuitRequested() const { return m_quit; }
    WindowRequest& Window() { return m_window; }
    Scene* CurrentScene() { return m_scene.get(); }
    const Scene* CurrentScene() const { return m_scene.get(); }
    const MachineConfig& Config() const { return m_config; }
    bool CursorHidden() const { return m_cursorHidden; }
    // Parsed .ent definitions (entities/<file>), cached by file name; null when
    // the file is missing or malformed.
    const EntityDef* EntityDefinition(const string& file);
    // The original pixel size of an image under the game root (0,0 if unreadable).
    vector2 ImageSize(const string& relativePath);
    // Invoke a named callback on an entity (Scene uses it); false if none.
    bool RunCallback(const string& name, ETHEntity entity);

    // The shared sprite cache 0.7.12 keyed by BASENAME (ETHResourceManager.cpp:
    // 63-118): LoadSprite, SetBackgroundImage and every entity's sprite, normal,
    // gloss and halo land in it; GetSpriteSize/DrawSprite look up by basename;
    // every LoadScene empties it. False when the image cannot be read.
    bool AddSpriteResource(const string& relativePath);
    // The frame length UnitsPerSecond uses: 1/60, or 0 on the frame of a load.
    float FrameSeconds() const { return m_frameSeconds; }
    // What an entity's final release stops its particle sounds through; renewed
    // at every load (see Entity::m_sfxBank).
    std::weak_ptr<SampleBank> SfxBank() const { return m_sfxBank; }

private:
    struct PendingLoad {
        string file, onLoad, onLoop;
        vector2 bucketSize{256.0f};
    };
    struct SpriteResource {
        string path;            // as loaded, relative to the game root
        vector2 size{0.0f};
    };

    void RunGuarded(const string& where, const std::function<void()>& fn);
    void DoLoad(const PendingLoad& request);
    void Render();
    void UpdateParticles(Entity& entity, uint slot);
    void RunCallbacks();
    const SpriteResource* FindSpriteResource(const string& path) const;

    MachineConfig m_config;
    std::map<string, ScriptFunction> m_functions;
    std::map<string, CallbackFunction> m_callbacks;

    std::unique_ptr<Scene> m_scene;
    string m_loopFunction;
    string m_sceneFileName;
    std::optional<PendingLoad> m_pendingLoad;

    uint m_frameIndex = 0;
    uint m_timeMs = 0;
    float m_frameSeconds = 0.0f;
    bool m_justLoaded = false;
    uint m_scriptAborts = 0;

    vector2 m_camera{0.0f};
    vector2 m_screenSize{1024.0f, 768.0f};
    bool m_roundUp = true;
    uint m_backgroundColor = 0xFF000000u;
    string m_backgroundImage;
    vector2 m_backgroundMin{0.0f};
    vector2 m_backgroundMax{0.0f};
    bool m_backgroundAdditive = false;
    bool m_pixelShaders = true;
    bool m_cursorHidden = false;
    bool m_quit = false;
    WindowRequest m_window;
    std::vector<videoMode> m_videoModes;

    std::vector<HudCmd> m_hudQueue;             // queued since the last render
    std::map<string, SpriteResource> m_sprites; // basename -> sprite
    std::map<string, std::unique_ptr<EntityDef>> m_definitions;
    std::map<string, vector2> m_imageSizes;

    std::vector<std::shared_ptr<Entity>> m_staticCallbacks;   // collected by Render
    std::set<string> m_loggedAborts;

    InputState m_input;
    SampleBank m_samples;
    std::shared_ptr<SampleBank> m_sfxBank;      // aliases m_samples; see SfxBank()
    Random m_rng;
    RenderSnapshot m_snapshot;
};

} // namespace Penumbra::Eth
