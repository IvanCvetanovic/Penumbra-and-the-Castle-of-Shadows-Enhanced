// The REAL ported game, played headless past level 1's first room: the menu,
// a melee fight, a ranged fire-resistant enemy, a potion, the hazards, a
// checkpoint and a death that returns to it, the level exit, the lives running
// out into the game-over screen, the king and the end screen with its high
// score, a Versus match to three points; then the wizard's other moves (double
// jump, light, both combos) against a knight, the co-op creature summoned by a
// second controller, the options screen's enhanced rows (E10); then the paths
// those leave out - level2 by the K_2 cheat and its play_sound markers,
// level2's own exit into level3, the paladin and the master knight, the
// summon's price and refusals, arenas 2 to 6 - and the menu's Quit. Last, a
// second runtime booted for a player with one gamepad and nothing else (E12),
// and a third for the options screen as a phone lays it out (E20).
//
// The harness is test_pn_boot's: one Machine (the script module's globals live
// for the whole program, as they lived for the whole of machine.exe),
// RegisterAll, Boot(ScriptMain), one Frame per InputFrame. Everything happens
// through the game's own logic. Where reaching a situation by playing would
// take minutes, the suite takes a SHORTCUT, and each one is named where it is
// taken, in the printed trace and in a comment:
//   - teleporting the wizard or the princess (SetPosition, forces zeroed);
//   - moving a spawn or play_sound MARKER next to the wizard, so that doLoop
//     spawns the enemy (plays the sound) exactly as it would have when the
//     camera reached it;
//   - writing a custom datum or a global the game itself writes (hp, mp,
//     g_lives), e.g. "the wizard is not at full health", "the king has lost
//     his last hit point".
// Nothing else is forced: AI, damage, deaths, fades, loads, saves are the
// game's.
//
// The chain of scenes is the game's own: menu -> level1 -> checkpoint ->
// level2 -> game over -> menu -> level3 (the original's K_3 cheat) -> end
// screen -> menu -> arena select -> pvp_lv1 -> menu -> level1 -> menu ->
// level2 (the K_2 cheat) -> level3 (level2's next_level) -> level1 ->
// pvp_lv2..pvp_lv6 -> menu. Each scenario still starts from a known state and,
// if the previous one did not leave it there, gets there by the game's own
// entry point (LoadScene with setupScene, newGame). New scenarios go at the
// end of the chain: the runtime reseeds rand() from the clock after frames
// with particles in view, so frames added earlier reshuffle every later roll.
//
// Findings about the ORIGINAL that the suite pins rather than fixes: the
// checkpoint save holds the checkpoint itself, so a respawn takes it again
// (main.as:184-186); the walk cycle restarts where two floor tiles meet,
// because doCharacterCollision keeps only the last collided box's thinner-box
// test (controlCharacters.as:126), leaving the wizard "in the air" for a frame
// (E11 fixes it); five horror.mp3 markers named "play_sound.ent" never play
// (setupScene.as:175 collects the exact name "play_sound"); a summon is
// refused only while the first creature is VISIBLE (controlCharacters.as:
// 688-697), so one off screen lets a second through for another life; and a
// level has no way back to the menu but ESC (setupScene.as:392), which a
// player with only a gamepad does not have.
//
// A sound recorder stands in for the speakers: every sample the game plays is
// logged with its frame and loop flag (see SoundLog for how long a one-shot
// "plays").

#include "script/Script.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <map>
#include <random>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include "TestHarness.hpp"

using namespace Penumbra::Eth;
namespace Script = Penumbra::Script;

namespace {

constexpr const char* kPlayer = "bruxo.ent";
constexpr const char* kPrincess = "princess.ent";

// The centres of the menu buttons' collision boxes in menu.esc (position plus
// the <Collision> offset (-18,4)); the menu's camera is at (0,0), so these are
// also the cursor's client coordinates.
const vector2 kVersusButton(466.0f, 271.0f);
const vector2 kNewGameButton(416.0f, 213.0f);
const vector2 kOptionsButton(414.0f, 446.0f);
const vector2 kRecordsButton(367.0f, 389.0f);
const vector2 kHowToPlayButton(381.0f, 327.0f);
const vector2 kCreditsButton(517.0f, 507.0f);
const vector2 kQuitButton(634.0f, 571.0f);
// arena_select.esc's thumbnail "1" (Obelisco) at (276,212), box offset (0,32).
const vector2 kArena1Thumbnail(276.0f, 244.0f);

// A one-shot sample counts as playing for this many frames (1.5 s); a looping
// one until it is stopped. The game reads IsSamplePlaying in a few places
// (pvpLoop's chefao restart, a chasing minion's sound, the death fade's music
// volume); none of them depends on a sample's true length.
constexpr uint kOneShotFrames = 90;

string BaseName(const string& path) {
    const auto slash = path.find_last_of("/\\");
    return slash == string::npos ? path : path.substr(slash + 1);
}

uint NowFrame() {
    const Machine* m = Machine::CurrentOrNull();
    return m != nullptr ? m->FrameIndex() : 0u;
}

// An AudioOut that logs every Play. Load succeeds only for a file that exists,
// so a sound the game names wrongly shows up as a failed load.
class SoundLog final : public AudioOut {
public:
    struct Voice {
        string file;
        bool loop = false;
        uint start = 0;
        bool stopped = false;
    };
    struct PlayEvent {
        string file;
        uint frame = 0;
        bool loop = false;
    };

    bool Load(const string& absolutePath, const bool music) override {
        (void)music;
        std::error_code ec;
        const bool ok = std::filesystem::is_regular_file(std::filesystem::path(absolutePath), ec);
        if (!ok) failedLoads.insert(absolutePath);
        return ok;
    }
    VoiceId Play(const string& absolutePath, const bool loop, const float volume, const float pan) override {
        (void)volume;
        (void)pan;
        const string file = BaseName(absolutePath);
        voices[++m_next] = Voice{file, loop, NowFrame(), false};
        plays.push_back(PlayEvent{file, NowFrame(), loop});
        return m_next;
    }
    void Stop(const VoiceId voice) override {
        const auto it = voices.find(voice);
        if (it != voices.end()) it->second.stopped = true;
    }
    void Set(const VoiceId voice, const float volume, const float pan) override {
        (void)voice;
        (void)volume;
        (void)pan;
    }
    bool IsPlaying(const VoiceId voice) override {
        const auto it = voices.find(voice);
        if (it == voices.end() || it->second.stopped) return false;
        return it->second.loop || NowFrame() < it->second.start + kOneShotFrames;
    }
    void UnloadAll() override {}

    // Whether `file` (a basename) was played on frame `since` or later.
    bool PlayedSince(const string& file, const uint since) const {
        for (const PlayEvent& e : plays) {
            if (e.file == file && e.frame >= since) return true;
        }
        return false;
    }
    // Whether `file` was played on exactly frame `frame`.
    bool PlayedAt(const string& file, const uint frame) const {
        for (const PlayEvent& e : plays) {
            if (e.file == file && e.frame == frame) return true;
        }
        return false;
    }
    // How many times `file` was played on frame `since` or later.
    uint PlayCount(const string& file, const uint since) const {
        uint n = 0;
        for (const PlayEvent& e : plays) {
            if (e.file == file && e.frame >= since) ++n;
        }
        return n;
    }
    // The newest voice of `file` loops and has not been stopped.
    bool Looping(const string& file) const {
        for (auto it = voices.rbegin(); it != voices.rend(); ++it) {
            if (it->second.file == file) return it->second.loop && !it->second.stopped;
        }
        return false;
    }

    std::map<VoiceId, Voice> voices;
    std::vector<PlayEvent> plays;
    std::set<string> failedLoads;

private:
    VoiceId m_next = 0;
};

struct Game {
    Machine& m;
    SoundLog& sound;
    string userRoot;
    // What the devices say when the suite presses nothing: the cursor, and a
    // pad that stays connected once plugged (InputState re-reads `connected`
    // every frame).
    InputFrame base;
    // Stand in for the layer's InputMapper::WarpCursor: a SetCursorPos the
    // scripts made this frame (menu.as:239, videoModes.as:61) becomes the next
    // frame's cursor, clamped to the screen as the OS cursor was. Off, the
    // requests are dropped and the suite places the cursor itself.
    bool warpCursor = false;

    void Step(const InputFrame& input) {
        m.Frame(input);
        vector2 warp(0.0f);
        if (m.Input().TakeCursorRequest(warp) && warpCursor) {
            const vector2 screen = m.GetScreenSize();
            warp.x = warp.x < 0.0f ? 0.0f : (warp.x > screen.x ? screen.x : warp.x);
            warp.y = warp.y < 0.0f ? 0.0f : (warp.y > screen.y ? screen.y : warp.y);
            base.cursor = warp;
            base.cursorAbsolute = warp;
        }
    }
    void Step() { Step(base); }
    void Steps(const uint n) {
        for (uint i = 0; i < n; ++i) Step();
    }
    void Steps(const uint n, const InputFrame& input) {
        for (uint i = 0; i < n; ++i) Step(input);
    }
    InputFrame With(std::initializer_list<KEY> keys) const {
        InputFrame f = base;
        for (const KEY k : keys) f.keys[static_cast<std::size_t>(k)] = true;
        return f;
    }
    uint Frame() const { return m.FrameIndex(); }
};

ETHEntity Player() { return SeekEntity(kPlayer); }
ETHEntity Princess() { return SeekEntity(kPrincess); }

bool Ready(const ETHEntity& e) {
    return e != nullptr && e->IsAlive() && e->GetIntData("hp") > 0 && e->CheckCustomData("deathTime") == DT_NODATA &&
           e->GetUIntData("touchingGround") != 0;
}

string Utf8(const string& cp1252) {
    // For the log only: Latin-1 bytes as UTF-8 (the few cp1252-only bytes the
    // game's strings hold print as '?').
    string out;
    for (const char ch : cp1252) {
        const auto c = static_cast<unsigned char>(ch);
        if (c < 0x80) {
            out += ch;
        } else if (c < 0xA0) {
            out += '?';
        } else {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

void Trace(const Game& g, const char* what) {
    const ETHEntity p = Player();
    const vector2 cam = GetCameraPos();
    if (p != nullptr) {
        const vector3 pos = p->GetPosition();
        std::printf("  [%5u] %-22s %-24s bruxo (%8.2f,%8.2f) hp %4d mp %3d ground %u  cam (%7.1f,%7.1f)  lives %2d  "
                    "aborts %u\n",
                    g.Frame(), what, GetSceneFileName().c_str(), pos.x, pos.y, p->GetIntData("hp"),
                    p->GetIntData("mp"), p->GetUIntData("touchingGround"), cam.x, cam.y, Script::g_lives,
                    g.m.ScriptAborts());
    } else {
        std::printf("  [%5u] %-22s %-24s (no bruxo)  cam (%7.1f,%7.1f)  lives %2d  aborts %u\n", g.Frame(), what,
                    GetSceneFileName().c_str(), cam.x, cam.y, Script::g_lives, g.m.ScriptAborts());
    }
}

bool HudHas(const Machine& m, const string& needle) {
    for (const HudCmd& c : m.Snapshot().hud) {
        if (c.kind == HudCmd::Kind::Text && c.text.find(needle) != string::npos) return true;
    }
    return false;
}

bool HudHasSprite(const Machine& m, const string& sprite) {
    for (const HudCmd& c : m.Snapshot().hud) {
        if ((c.kind == HudCmd::Kind::Sprite || c.kind == HudCmd::Kind::ShapedSprite) &&
            c.sprite.find(sprite) != string::npos) {
            return true;
        }
    }
    return false;
}

// Steps until `pred` holds (checked before each step), at most `limit` steps.
// Returns the number of steps taken, or -1.
int WaitFor(Game& g, const uint limit, const std::function<bool()>& pred, const InputFrame* input = nullptr) {
    for (uint i = 0;; ++i) {
        if (pred()) return static_cast<int>(i);
        if (i == limit) return -1;
        g.Step(input != nullptr ? *input : g.base);
    }
}

// HUD text queued by a callback is drawn by the NEXT frame's render, so a
// message is looked for over a few frames.
bool WaitForHud(Game& g, const string& needle, const uint limit, const InputFrame* input = nullptr) {
    return WaitFor(g, limit, [&] { return HudHas(g.m, needle); }, input) >= 0;
}

ETHEntity WaitForPlayerReady(Game& g, const uint limit, const char* what) {
    const int n = WaitFor(g, limit, [] { return Ready(Player()); });
    if (n < 0) {
        std::printf("  %s: the wizard is not standing after %u frames\n", what, limit);
        Trace(g, what);
    }
    return Player();
}

// Entities named `name` created since `sinceId` (ids only grow).
ETHEntityArray NewEntities(const string& name, const int sinceId) {
    ETHEntityArray all;
    GetEntityArray(name, all);
    ETHEntityArray out;
    for (const ETHEntity& e : all) {
        if (e->GetID() >= sinceId) out.push_back(e);
    }
    return out;
}

// Remembers every new entity of the watched names, frame by frame (particle
// temporaries such as hit_fail.ent can come and go between two checks).
struct Spotter {
    int sinceId = 0;
    std::vector<string> names;
    std::map<string, std::map<int, ETHEntity>> seen;
    std::map<string, uint> firstFrame;

    Spotter(const int since, std::initializer_list<string> watched) : sinceId(since), names(watched) {}
    void Poll() {
        for (const string& name : names) {
            for (const ETHEntity& e : NewEntities(name, sinceId)) {
                if (seen[name].emplace(e->GetID(), e).second && firstFrame.count(name) == 0) {
                    firstFrame[name] = NowFrame();
                }
            }
        }
    }
    uint Count(const string& name) const {
        const auto it = seen.find(name);
        return it == seen.end() ? 0u : static_cast<uint>(it->second.size());
    }
    ETHEntity First(const string& name) const {
        const auto it = seen.find(name);
        if (it == seen.end() || it->second.empty()) return nullptr;
        return it->second.begin()->second;
    }
};

float Dist(const ETHEntity& a, const ETHEntity& b) {
    const vector2 d = a->GetPositionXY() - b->GetPositionXY();
    return std::sqrt(d.x * d.x + d.y * d.y);
}

void Teleport(const ETHEntity& e, const vector2& xy) {
    const vector3 p = e->GetPosition();
    e->SetPosition(vector3(xy.x, xy.y, p.z));
    e->AddFloatData("forceX", 0.0f);
    e->AddFloatData("forceY", 0.0f);
    e->AddFloatData("knockBackX", 0.0f);
    e->AddFloatData("knockBackY", 0.0f);
}

string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return string();
    return string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool FileExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

// data.enml's global.lv1..lv30 (lv20 is missing; E7 takes lv19's).
int Threshold(const int level) {
    static const int kLevels[] = {200,   400,   800,   1600,  3200,  4000,  5000,  6000,  7000,  8000,
                                  9000,  10000, 10000, 11000, 11000, 11000, 11000, 11000, 11000, 11000,
                                  11000, 11000, 11000, 11000, 11000, 11000, 11000, 11000, 11000, 11000};
    if (level < 1) return 1;
    return level <= 30 ? kLevels[level - 1] : 11000;
}

// util.as:406 addToExp, independently of the port.
void ExpectedExp(int& exp, int& level, const int gain) {
    exp += gain;
    int next = Threshold(level);
    while (exp >= next) {
        const int diff = exp - next;
        ++level;
        next = Threshold(level);
        exp = diff;
    }
}

// doDamage.as:83-87: a main character's blow, (d + int(d*(level/4))) * int(multiplier).
int MainCharDamage(const int damage, const int level) {
    return damage + static_cast<int>(static_cast<float>(damage) * (static_cast<float>(level) / 4.0f));
}

ETHEntity FindMarker(const string& name, const int preferredId) {
    ETHEntity found;
    for (const ETHEntity& marker : Script::g_spawn) {
        if (!marker->IsAlive() || marker->GetStringData("name") != name) continue;
        if (marker->GetID() == preferredId) return marker;
        if (found == nullptr) found = marker;
    }
    return found;
}

// A level loaded the way the game loads one (newGame, a death, next_level):
// LoadScene with setupScene and levelLoop. Waits for setupScene to have run -
// the scene's NAME does not change when the same scene is reloaded - and for
// the wizard to stand.
ETHEntity LoadLevel(Game& g, const string& scene) {
    std::printf("  (%s by LoadScene(..., \"setupScene\", \"levelLoop\"))\n", scene.c_str());
    const uint setup = Script::g_levelStartTime;
    LoadScene(scene, "setupScene", "levelLoop");
    const int loaded = WaitFor(g, 3, [&] { return Script::g_levelStartTime != setup; });
    CHECK(loaded >= 0);
    CHECK(GetSceneFileName() == scene);
    return WaitForPlayerReady(g, 240, scene.c_str());
}

// That level, unless it is already running with the wizard alive.
ETHEntity EnsureLevel(Game& g, const string& scene) {
    const ETHEntity p = Player();
    if (GetSceneFileName() == scene && p != nullptr && p->GetIntData("hp") > 0 &&
        p->CheckCustomData("deathTime") == DT_NODATA) {
        return WaitForPlayerReady(g, 240, scene.c_str());
    }
    return LoadLevel(g, scene);
}

bool EnsureMenu(Game& g) {
    if (GetSceneFileName() == "scenes/menu.esc") return true;
    std::printf("  (back to the menu by ESC)\n");
    g.Step(g.With({K_ESC}));
    WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/menu.esc"; });
    if (GetSceneFileName() != "scenes/menu.esc") {
        std::printf("  (ESC did not reach the menu; LoadScene(menu))\n");
        LoadScene("scenes/menu.esc", "menuPreLoop", "menuLoop", vector2(1024.0f, 256.0f));
        WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/menu.esc"; });
    }
    g.Steps(5);
    return GetSceneFileName() == "scenes/menu.esc";
}

string LastButton() {
    const ETHEntity cursor = SeekEntity("cursor.ent");
    return cursor != nullptr ? cursor->GetStringData("lastButton") : string("(no cursor)");
}

// Frames from now until the scene's setupScene runs again (a reload of the same
// scene or another level), or -1.
int WaitForSetup(Game& g, const uint limit, const InputFrame* input = nullptr) {
    const uint start = Script::g_levelStartTime;
    return WaitFor(g, limit, [&] { return Script::g_levelStartTime != start; }, input);
}

// --- Scenario bookkeeping ------------------------------------------------------------

struct Result {
    string name;
    int failures = 0;
    uint aborts = 0;
    bool threw = false;
};
std::vector<Result> g_results;

void RunScenario(Game& g, const char* name, const std::function<void(Game&)>& body) {
    std::printf("\n=== %s\n", name);
    const int failuresBefore = test::g_failures;
    const uint abortsBefore = g.m.ScriptAborts();
    Result r;
    r.name = name;
    try {
        body(g);
    } catch (const std::exception& e) {
        std::printf("  THREW: %s\n", e.what());
        r.threw = true;
        CHECK_MSG(false, string("the scenario threw: ") + e.what());
    }
    r.failures = test::g_failures - failuresBefore;
    r.aborts = g.m.ScriptAborts() - abortsBefore;
    Trace(g, "end of scenario");
    std::printf("=== %s: %s (%d failed checks, %u new script aborts)\n", name,
                (r.failures == 0 && r.aborts == 0 && !r.threw) ? "PASS" : "FAIL", r.failures, r.aborts);
    CHECK_EQ(r.aborts, 0u);
    g_results.push_back(r);
}

// === 10. Menu navigation ====================================================================

void ScenarioMenu(Game& g) {
    WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/menu.esc"; });
    CHECK(GetSceneFileName() == "scenes/menu.esc");
    g.Steps(30);
    CHECK(g.sound.Looping("menu.mp3"));

    std::printf("-- Versus without a second controller\n");
    g.base.cursor = kVersusButton;
    g.Steps(3);
    std::printf("  cursor over '%s'\n", LastButton().c_str());
    CHECK(LastButton() == "versus");
    CHECK(WaitForHud(g, "Jogador versus Jogador", 3));
    // menu.as:278: the refusal names the joystick it needs.
    CHECK(HudHas(g.m, "ao menos um joystick"));
    const uint pressed = g.Frame() + 1;
    g.Step(g.With({K_RETURN}));
    g.Steps(2);
    std::printf("  Enter on Versus: fail.ogg played %s, scene %s\n",
                g.sound.PlayedSince("fail.ogg", pressed) ? "yes" : "NO", GetSceneFileName().c_str());
    CHECK(g.sound.PlayedSince("fail.ogg", pressed));
    const ETHEntity cursor = SeekEntity("cursor.ent");
    CHECK(cursor != nullptr);
    if (cursor != nullptr) CHECK(cursor->CheckCustomData("newGame") == DT_NODATA);
    g.Steps(200);
    CHECK(GetSceneFileName() == "scenes/menu.esc");

    std::printf("-- the menu's panels\n");
    g.base.cursor = kHowToPlayButton;
    g.Steps(3);
    std::printf("  cursor over '%s'\n", LastButton().c_str());
    CHECK(LastButton() == "como_jogar");
    CHECK(WaitForHud(g, "Como Jogar", 3));
    CHECK(HudHas(g.m, "Ataque/espada: S"));
    g.base.cursor = kCreditsButton;
    g.Steps(3);
    std::printf("  cursor over '%s'\n", LastButton().c_str());
    CHECK(LastButton() == "creditos");
    CHECK(WaitForHud(g, "Andr\xE9 Santee", 3));
    // E21: the enhanced edition's credit, after the original team's.
    CHECK(HudHas(g.m, "-Taina Monclaire\r\n\r\nIvan Cvetanovi\x8D\r\n -Edi\xE7\xE3o aprimorada"));
    g.base.cursor = kRecordsButton;
    g.Steps(3);
    CHECK(LastButton() == "melhores_tempos");
    // No record yet: the shipped hs.enml's five 59:59.
    CHECK(WaitForHud(g, "1    59:59", 3));

    std::printf("-- the options screen, and back\n");
    // The layer reports the display's modes; the screen lists the 32-bit ones
    // of at least 800x600 (videoModes.as:100).
    g.m.SetVideoModes({videoMode{640, 480, PF32BIT}, videoMode{800, 600, PF32BIT}, videoMode{1024, 768, PF16BIT},
                       videoMode{1024, 768, PF32BIT}, videoMode{1280, 1024, PF32BIT}});
    g.base.cursor = kOptionsButton;
    g.Steps(3);
    std::printf("  cursor over '%s'\n", LastButton().c_str());
    CHECK(LastButton() == "opcoes_de_video");
    CHECK(WaitForHud(g, "Configura\xE7\xF5" "es", 3));
    g.Step(g.With({K_RETURN}));
    const int optionsAfter = WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/videoModes.esc"; });
    std::printf("  videoModes.esc loaded %d frames after the press\n", optionsAfter);
    CHECK(optionsAfter >= 0);
    CHECK(WaitForHud(g, "Op\xE7\xF5" "es de v\xED" "deo", 4));
    CHECK(SeekEntity("picker") != nullptr);
    g.Steps(3);
    CHECK(HudHas(g.m, "800x600x32"));
    CHECK(HudHas(g.m, "1024x768x32"));
    CHECK(HudHas(g.m, "1280x1024x32"));
    CHECK(!HudHas(g.m, "640x480"));
    CHECK(!HudHas(g.m, "x16"));
    // Each option is a click: the cursor over it, then a fresh Enter.
    const auto click = [&g](const vector2& at) {
        g.base.cursor = at;
        g.Steps(2);
        g.Step(g.With({K_RETURN}));
        g.Steps(2);
    };
    // 800x600 at (30,100), 1024x768 at (30,125), 1280x1024 at (30,150), 25 px tall.
    click(vector2(60.0f, 162.0f));
    std::printf("  clicked 1280x1024x32: window request %ux%u\n", g.m.Window().width, g.m.Window().height);
    CHECK_EQ(g.m.Window().width, 1280u);
    CHECK_EQ(g.m.Window().height, 1024u);
    // g_enablePS at (255,100), g_windowed at (255,170): 25 px lines.
    click(vector2(300.0f, 137.0f));
    std::printf("  'Desativa pixel shaders': switch %u, the snapshot's pixel shaders %s\n",
                Script::g_enablePS.getCurrent(),
                g.m.Snapshot().pixelShaders ? "on" : "off");
    CHECK_EQ(Script::g_enablePS.getCurrent(), 1u);
    CHECK(!g.m.Snapshot().pixelShaders);
    click(vector2(300.0f, 112.0f));
    CHECK_EQ(Script::g_enablePS.getCurrent(), 0u);
    CHECK(g.m.Snapshot().pixelShaders);
    click(vector2(300.0f, 207.0f));
    std::printf("  'Tela-cheia': switch %u, window request windowed %s\n", Script::g_windowed.getCurrent(),
                g.m.Window().windowed ? "yes" : "no");
    CHECK_EQ(Script::g_windowed.getCurrent(), 1u);
    CHECK(!g.m.Window().windowed);
    click(vector2(300.0f, 182.0f));
    CHECK_EQ(Script::g_windowed.getCurrent(), 0u);
    CHECK(g.m.Window().windowed);
    // g_controls at (255,260): two images, each as tall as its sprite.
    const vector2 option = GetSpriteSize("interface/input_options1.png");
    std::printf("  input_options1.png %.0fx%.0f\n", option.x, option.y);
    CHECK(option.y > 0.0f);
    click(vector2(270.0f, 262.0f + option.y + 2.0f));
    CHECK_EQ(Script::g_controls.getCurrent(), 1u);
    click(vector2(270.0f, 264.0f));
    CHECK_EQ(Script::g_controls.getCurrent(), 0u);
    g.Steps(10);
    g.Step(g.With({K_ESC}));
    const int backAfter = WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/menu.esc"; });
    std::printf("  ESC: menu.esc loaded %d frames later\n", backAfter);
    CHECK(backAfter >= 0);
    g.Steps(10);

    std::printf("-- New game: the cursor on novo_jogo and Enter\n");
    g.base.cursor = kNewGameButton;
    g.Steps(3);
    std::printf("  cursor over '%s'\n", LastButton().c_str());
    CHECK(LastButton() == "novo_jogo");
    const uint press = g.Frame() + 1;
    g.Step(g.With({K_RETURN}));
    const ETHEntity cursor2 = SeekEntity("cursor.ent");
    CHECK(cursor2 != nullptr);
    if (cursor2 != nullptr) {
        CHECK(cursor2->CheckCustomData("newGame") == DT_UINT);
        CHECK(cursor2->GetStringData("scene") == "CAMPAIGN");
    }
    CHECK(g.sound.PlayedSince("newgame.mp3", press));
    bool loading = false;
    const int loadedAfter = WaitFor(g, 240, [&] {
        loading = loading || HudHas(g.m, "Carregando...");
        return GetSceneFileName() == "scenes/level1.esc";
    });
    std::printf("  level1.esc loaded %d frames after the press (the fade is 3000 ms = 180 frames); "
                "'Carregando...' shown during the fade: %s\n",
                loadedAfter, loading ? "yes" : "NO");
    CHECK(loadedAfter >= 180 && loadedAfter <= 183);
    CHECK(loading);
    const ETHEntity p = WaitForPlayerReady(g, 240, "level1 start");
    CHECK(Ready(p));
    Trace(g, "level1 running");
    // story 811 "story01" at (802,242) is on screen: data.enml's text, drawn
    // where it stands (main.as:163-168).
    CHECK(WaitForHud(g, "Pelos corredores subterr", 3));
    CHECK(HudHas(g.m, "hp: 100"));
    CHECK_EQ(Script::g_lives, 13);
    CHECK_EQ(Script::g_exp[0], 0);
    CHECK_EQ(Script::g_charLevel[0], 1);
}

// === 1. Enemy melee ==========================================================================

void ScenarioMelee(Game& g) {
    ETHEntity p = Player();
    if (GetSceneFileName() != "scenes/level1.esc" || !Ready(p)) {
        std::printf("  (not in a fresh level1: newGame(\"CAMPAIGN\"))\n");
        const uint setup = Script::g_levelStartTime;
        Script::newGame("CAMPAIGN");
        WaitFor(g, 3, [&] { return Script::g_levelStartTime != setup; });
    }
    p = WaitForPlayerReady(g, 240, "melee");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard in level1");
        return;
    }
    const int exp0 = Script::g_exp[0];
    const int exp1 = Script::g_exp[1];
    const int level0 = Script::g_charLevel[0];
    const int level1 = Script::g_charLevel[1];

    // SHORTCUT: warrior marker 791 (280,-1020) is moved 120 px to the wizard's
    // right; doLoop spawns it on the next frame, as it would once on screen.
    const ETHEntity marker = FindMarker("warrior", 791);
    CHECK(marker != nullptr);
    if (marker == nullptr) return;
    const vector2 wiz = p->GetPositionXY();
    std::printf("  SHORTCUT: warrior marker %d moved to (%.0f,%.0f)\n", marker->GetID(), wiz.x + 120.0f,
                wiz.y - 20.0f);
    const int since = GetLastID();
    marker->SetPosition(vector3(wiz.x + 120.0f, wiz.y - 20.0f, 0.0f));
    ETHEntity warrior;
    WaitFor(g, 3, [&] {
        const ETHEntityArray found = NewEntities("warrior.ent", since);
        if (!found.empty()) warrior = found[0];
        return warrior != nullptr;
    });
    CHECK(warrior != nullptr);
    if (warrior == nullptr) return;
    CHECK(!marker->IsAlive());
    std::printf("  warrior id %d spawned at (%.1f,%.1f): hp %d damage %d speed %.0f viewRadius %.0f attackRadius "
                "%.0f coolDown %u expGiven %d\n",
                warrior->GetID(), warrior->GetPosition().x, warrior->GetPosition().y, warrior->GetIntData("hp"),
                warrior->GetIntData("damage"), warrior->GetFloatData("speed"), warrior->GetFloatData("viewRadius"),
                warrior->GetFloatData("attackRadius"), warrior->GetUIntData("coolDown"),
                warrior->GetIntData("expGiven"));
    // spawn() from data.enml's [warrior] (setupScene.as:52-110).
    CHECK_EQ(warrior->GetIntData("hp"), 75);
    CHECK_EQ(warrior->GetIntData("damage"), 5);
    CHECK_EQ(warrior->GetIntData("expGiven"), 75);
    CHECK_EQ(warrior->GetUIntData("fireResistant"), 0u);

    // The approach: STANDING -> CHASING (within viewRadius), then the sword.
    std::printf("-- the warrior chases and attacks; the wizard stands still\n");
    Spotter spot(since, {"enemy_sword.ent", "fade_out_beam.ent", "blood.ent", "enemy_blood.ent"});
    const float startX = warrior->GetPosition().x;
    bool sawChasing = false;
    int hpBefore = p->GetIntData("hp");
    std::vector<int> wizardDrops;
    const int firstHit = WaitFor(g, 400, [&] {
        spot.Poll();
        if (warrior->GetUIntData("action") == Script::CHASING) sawChasing = true;
        const int hp = p->GetIntData("hp");
        if (hp < hpBefore) wizardDrops.push_back(hpBefore - hp);
        hpBefore = hp;
        return !wizardDrops.empty();
    });
    std::printf("  first hit on the wizard after %d frames: -%d hp; warrior x %.1f -> %.1f, chasing seen %s, "
                "enemy_sword.ent seen %u\n",
                firstHit, wizardDrops.empty() ? 0 : wizardDrops[0], startX, warrior->GetPosition().x,
                sawChasing ? "yes" : "NO", spot.Count("enemy_sword.ent"));
    CHECK(firstHit > 0);
    CHECK(sawChasing);
    CHECK(startX - warrior->GetPosition().x > 30.0f);
    CHECK(spot.Count("enemy_sword.ent") >= 1u);
    // data.enml warrior damage 5, times int(1) (doDamage.as:91).
    if (!wizardDrops.empty()) CHECK_EQ(wizardDrops[0], 5);
    // The blow's number floats over him, and his bar says so (messageManager.as:70,
    // interface.as:65).
    CHECK(WaitForHud(g, "-5", 3));
    CHECK(WaitForHud(g, "hp: 95", 3));

    std::printf("-- the wizard answers with the sword (S)\n");
    const int swordDamage = MainCharDamage(20, level0);
    std::vector<int> warriorDrops;
    int warriorHp = warrior->GetIntData("hp");
    uint lastPress = 0;
    uint presses = 0;
    for (uint i = 0; i < 900 && warrior->GetIntData("hp") > 0; ++i) {
        const float dx = warrior->GetPosition().x - p->GetPosition().x;
        const bool press = dx > 0.0f && dx < 50.0f && g.Frame() >= lastPress + 15 && p->GetIntData("hp") > 0;
        if (press) {
            lastPress = g.Frame();
            ++presses;
            g.Step(g.With({K_S}));
        } else {
            g.Step();
        }
        spot.Poll();
        const int hp = warrior->GetIntData("hp");
        if (hp < warriorHp) warriorDrops.push_back(warriorHp - hp);
        warriorHp = hp;
        const int whp = p->GetIntData("hp");
        if (whp < hpBefore) wizardDrops.push_back(hpBefore - whp);
        hpBefore = whp;
    }
    std::printf("  %u presses; warrior hp drops:", presses);
    for (const int d : warriorDrops) std::printf(" -%d", d);
    std::printf(" (expected -%d each: (20 + int(20*%d/4)) * 1); wizard hp drops:", swordDamage, level0);
    for (const int d : wizardDrops) std::printf(" -%d", d);
    std::printf("\n");
    CHECK(warrior->GetIntData("hp") <= 0);
    // 75 hp at 25 a blow: three blows, the last one taking it to 0 (addToHp clamps).
    CHECK_EQ(static_cast<uint>(warriorDrops.size()), 3u);
    for (const int d : warriorDrops) CHECK_EQ(d, swordDamage);
    for (const int d : wizardDrops) CHECK_EQ(d, 5);

    std::printf("-- death: fade, fade_out_beam, experience, deletion\n");
    const int deathAt = WaitFor(g, 3, [&] {
        spot.Poll();
        return warrior->CheckCustomData("deathTime") != DT_NODATA;
    });
    CHECK(deathAt >= 0);
    const uint deathFrame = g.Frame();
    CHECK(!warrior->Collidable());
    CHECK(warrior->GetColor() == vector3(0.0f));
    CHECK(spot.Count("fade_out_beam.ent") >= 1u);
    int e0 = exp0, l0 = level0, e1 = exp1, l1 = level1;
    ExpectedExp(e0, l0, 75);
    ExpectedExp(e1, l1, 75);
    std::printf("  exp: player0 %d -> %d (level %d), player1 %d -> %d (level %d); expected %d/%d and %d/%d\n", exp0,
                Script::g_exp[0], Script::g_charLevel[0], exp1, Script::g_exp[1], Script::g_charLevel[1], e0, l0,
                e1, l1);
    CHECK_EQ(Script::g_exp[0], e0);
    CHECK_EQ(Script::g_exp[1], e1);
    CHECK_EQ(Script::g_charLevel[0], l0);
    CHECK_EQ(Script::g_charLevel[1], l1);
    g.Steps(90);
    const float midAlpha = warrior->GetAlpha();
    const int gone = WaitFor(g, 120, [&] { return !warrior->IsAlive(); });
    const uint deletedAfter = g.Frame() - deathFrame;
    std::printf("  alpha at +90 frames %.3f; deleted %u frames after its death frame (DEAD_FADE_OUT_TIME 3000 ms)\n",
                midAlpha, deletedAfter);
    CHECK(gone >= 0);
    CHECK(midAlpha > 0.4f && midAlpha < 0.6f);
    CHECK_EQ(deletedAfter, 180u);
}

// === 7. Potion =================================================================================

void ScenarioPotion(Game& g) {
    const ETHEntity p = EnsureLevel(g, "scenes/level1.esc");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard");
        return;
    }
    // level1's potion_small id 195 at (1844,413), hp 20.
    const ETHEntity potion = SeekEntity(195);
    CHECK(potion != nullptr);
    if (potion == nullptr) return;
    CHECK(potion->GetEntityName() == "potion_small.ent");
    CHECK_EQ(potion->GetIntData("hp"), 20);
    const int maxHp = p->GetIntData("maxHp");

    std::printf("-- at full health the potion stays\n");
    // SHORTCUT: the wizard is teleported onto the potion, at full health.
    p->AddIntData("hp", maxHp);
    std::printf("  SHORTCUT: wizard hp set to maxHp %d and teleported onto potion 195\n", maxHp);
    Teleport(p, potion->GetPositionXY() + vector2(0.0f, -12.0f));
    g.Steps(20);
    const float dist = Dist(p, potion);
    Trace(g, "on the potion");
    std::printf("  distance wizard-potion %.2f (the potion takes a main character within 20)\n", dist);
    CHECK(dist < 20.0f);
    CHECK(potion->IsAlive());
    CHECK_EQ(p->GetIntData("hp"), maxHp);

    // help 196 "Caveiras recuperam seu HP" stands beside this potion.
    CHECK(WaitForHud(g, "Caveiras recuperam seu HP", 3));
    std::printf("-- below full health it heals 20 and is gone\n");
    // SHORTCUT: the wizard's hp set to 50 (he has been hurt).
    p->AddIntData("hp", 50);
    std::printf("  SHORTCUT: wizard hp set to 50\n");
    const int since = GetLastID();
    const int took = WaitFor(g, 10, [&] { return !potion->IsAlive(); });
    std::printf("  potion taken after %d frames; hp %d; potion_pick.ent new %u\n", took, p->GetIntData("hp"),
                static_cast<uint>(NewEntities("potion_pick.ent", since).size()));
    CHECK(took >= 0);
    CHECK_EQ(p->GetIntData("hp"), 70);
    CHECK(!NewEntities("potion_pick.ent", since).empty());
}

// === 6. Hazards: the lava shooter and instant death ===========================================

void ScenarioShooterAndLava(Game& g) {
    const ETHEntity p = EnsureLevel(g, "scenes/level1.esc");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard");
        return;
    }
    // level1's shooter id 81 at (1632,500) over lava (instant_death.ent at (1632,544)).
    const ETHEntity shooter = SeekEntity(81);
    CHECK(shooter != nullptr);
    if (shooter == nullptr) return;
    CHECK(shooter->GetEntityName() == "shooter.ent");

    std::printf("-- a lava shooter's fire_shoot hits the wizard\n");
    // SHORTCUT: the wizard is held in the air 50 px above the shooter, every
    // frame, until a shot reaches him (the shooter's own cooldown picks when).
    if (p->GetIntData("hp") >= p->GetIntData("maxHp")) p->AddIntData("hp", 70);
    const vector2 hover = shooter->GetPositionXY() + vector2(0.0f, -50.0f);
    std::printf("  SHORTCUT: wizard held at (%.0f,%.0f) until hit\n", hover.x, hover.y);
    const int since = GetLastID();
    const uint start = g.Frame();
    Spotter spot(since, {"fire_shoot.ent", "explosion.ent", "lava_drops.ent"});
    int hp = p->GetIntData("hp");
    int drop = 0;
    for (uint i = 0; i < 480 && drop == 0; ++i) {
        Teleport(p, hover);
        g.Step();
        spot.Poll();
        const int now = p->GetIntData("hp");
        drop = hp - now;
        hp = now;
    }
    const ETHEntity shot = spot.First("fire_shoot.ent");
    std::printf("  fire_shoot.ent spawned %u (first on frame +%u), hp drop %d, explosion.ent %u, "
                "cast_fire_spell.ogg played %s\n",
                spot.Count("fire_shoot.ent"),
                spot.firstFrame.count("fire_shoot.ent") != 0 ? spot.firstFrame["fire_shoot.ent"] - start : 0u, drop,
                spot.Count("explosion.ent"), g.sound.PlayedSince("cast_fire_spell.ogg", start) ? "yes" : "NO");
    CHECK(spot.Count("fire_shoot.ent") >= 1u);
    // lavaShooter.as:58: damage 18.
    CHECK_EQ(drop, 18);
    CHECK(spot.Count("explosion.ent") >= 1u);
    CHECK(g.sound.PlayedSince("cast_fire_spell.ogg", start));
    if (shot != nullptr) {
        CHECK_EQ(shot->GetIntData("damage"), 18);
    }

    std::printf("-- let go: he falls into the lava (instant_death.ent)\n");
    const int lives = Script::g_lives;
    const int hpBeforeFall = p->GetIntData("hp");
    const int died = WaitFor(g, 120, [&] { return p->CheckCustomData("deathTime") != DT_NODATA; });
    const uint deathFrame = g.Frame();
    Trace(g, "in the lava");
    std::printf("  dead %d frames after letting go (hp %d -> %d); lava at (1632,544) 64x64\n", died, hpBeforeFall,
                p->GetIntData("hp"));
    CHECK(died > 0);
    CHECK_EQ(p->GetIntData("hp"), 0);
    CHECK(p->GetPosition().y > hover.y);
    const int reloaded = WaitForSetup(g, 200);
    std::printf("  scene set up again %d frames after the death frame (+%u since): %s, lives %d -> %d\n", reloaded,
                g.Frame() - deathFrame, GetSceneFileName().c_str(), lives, Script::g_lives);
    CHECK(reloaded >= 179 && reloaded <= 182);
    CHECK(GetSceneFileName() == "scenes/level1.esc");
    CHECK_EQ(Script::g_lives, lives - 1);
    const ETHEntity again = WaitForPlayerReady(g, 240, "respawn");
    CHECK(Ready(again));
    if (again != nullptr) {
        std::printf("  respawned at (%.1f,%.1f) hp %d\n", again->GetPosition().x, again->GetPosition().y,
                    again->GetIntData("hp"));
        CHECK(std::fabs(again->GetPosition().x - 410.0f) < 2.0f);
        CHECK_EQ(again->GetIntData("hp"), 100);
        CHECK(again->CheckCustomData("hasCheckpoint") == DT_NODATA);
    }
    g.Steps(30);
    CHECK_EQ(Script::g_lives, lives - 1);
}

// === 3. Checkpoint ===============================================================================

void ScenarioCheckpoint(Game& g) {
    const ETHEntity p = EnsureLevel(g, "scenes/level1.esc");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard");
        return;
    }
    const std::filesystem::path userSave = std::filesystem::path(g.userRoot) / "scenes" / "checkpoint.esc";
    const std::filesystem::path originalSave =
        std::filesystem::path(PENUMBRA_ORIGINAL_DIR) / "scenes" / "checkpoint.esc";
    CHECK(!FileExists(userSave));
    CHECK(!FileExists(originalSave));

    // level1's checkpoint id 201 at (3367,413).
    const ETHEntity cp = SeekEntity(201);
    CHECK(cp != nullptr);
    if (cp == nullptr) return;
    CHECK(cp->GetEntityName() == "checkpoint.ent");
    const int lives = Script::g_lives;

    std::printf("-- touch it\n");
    // SHORTCUT: the wizard is teleported onto the checkpoint.
    std::printf("  SHORTCUT: wizard teleported onto checkpoint 201\n");
    const int since = GetLastID();
    Teleport(p, cp->GetPositionXY() + vector2(0.0f, -10.0f));
    const int took = WaitFor(g, 10, [&] { return !cp->IsAlive(); });
    const vector3 savedPos = p->GetPosition();
    const int savedHp = p->GetIntData("hp");
    std::printf("  checkpoint taken %d frames after the teleport; wizard at (%.1f,%.1f) hp %d\n", took, savedPos.x,
                savedPos.y, savedHp);
    CHECK(took >= 0);
    CHECK(p->CheckCustomData("hasCheckpoint") == DT_UINT);
    CHECK_EQ(p->GetUIntData("hasCheckpoint"), 1u);
    CHECK_EQ(p->GetIntData("lives"), lives);
    CHECK(!NewEntities("checkpoint_effect.ent", since).empty());
    CHECK(FileExists(userSave));
    CHECK(!FileExists(originalSave));
    const string saved = ReadFile(userSave);
    std::printf("  %s: %zu bytes; bruxo.ent in it %s, hasCheckpoint in it %s\n", userSave.generic_string().c_str(),
                saved.size(), saved.find("<EntityName>bruxo.ent</EntityName>") != string::npos ? "yes" : "NO",
                saved.find("hasCheckpoint") != string::npos ? "yes" : "NO");
    CHECK(saved.find("<EntityName>bruxo.ent</EntityName>") != string::npos);
    CHECK(saved.find("hasCheckpoint") != string::npos);
    CHECK(WaitForHud(g, "Checkpoint...", 3));

    std::printf("-- die (hp 0): 3 s fade, one life, back at the checkpoint\n");
    // SHORTCUT: the wizard's hp set to 0.
    std::printf("  SHORTCUT: wizard hp set to 0\n");
    g.Steps(5);
    const int livesBeforeDeath = Script::g_lives;
    p->AddIntData("hp", 0);
    bool fadeDrawn = false;
    const int reloaded = WaitFor(g, 200, [&] {
        for (const HudCmd& c : g.m.Snapshot().hud) {
            if (c.kind == HudCmd::Kind::Rectangle && c.size == GetScreenSize() && (c.color >> 24) > 0x80u) {
                fadeDrawn = true;
            }
        }
        return GetSceneFileName() == "scenes/checkpoint.esc";
    });
    std::printf("  scenes/checkpoint.esc loaded %d frames after hp 0 (lives %d -> %d); fade drawn %s\n", reloaded,
                livesBeforeDeath, Script::g_lives, fadeDrawn ? "yes" : "NO");
    CHECK(reloaded >= 181 && reloaded <= 183);
    CHECK(fadeDrawn);
    CHECK_EQ(Script::g_lives, livesBeforeDeath - 1);

    const ETHEntity back = Player();
    CHECK(back != nullptr);
    if (back == nullptr) return;
    std::printf("  wizard back at (%.1f,%.1f) (saved at (%.1f,%.1f)), hp %d (saved %d), hasCheckpoint %u\n",
                back->GetPosition().x, back->GetPosition().y, savedPos.x, savedPos.y, back->GetIntData("hp"), savedHp,
                back->GetUIntData("hasCheckpoint"));
    CHECK(std::fabs(back->GetPosition().x - savedPos.x) < 3.0f);
    CHECK(std::fabs(back->GetPosition().y - savedPos.y) < 3.0f);
    // The saved custom data came back with its types.
    CHECK(back->CheckCustomData("hasCheckpoint") == DT_UINT);
    CHECK(back->CheckCustomData("hp") == DT_INT);
    CHECK(back->CheckCustomData("maxHp") == DT_INT);
    CHECK(back->CheckCustomData("mp") == DT_INT);
    CHECK(back->CheckCustomData("speed") == DT_FLOAT);
    CHECK(back->CheckCustomData("jumpForce") == DT_FLOAT);
    CHECK(back->CheckCustomData("playerId") == DT_UINT);
    CHECK(back->CheckCustomData("lives") == DT_INT);
    CHECK_EQ(back->GetIntData("hp"), savedHp);
    CHECK_NEAR(back->GetFloatData("speed"), 150.0f);
    CHECK_NEAR(back->GetFloatData("jumpForce"), 507.0f);

    // main.as:184 saves BEFORE deleting the checkpoint, so the save holds it and
    // the respawn stands on it: it triggers again (the original's behaviour).
    const ETHEntity cpAgain = SeekEntity(201);
    std::printf("  checkpoint 201 in the reloaded scene: %s\n",
                cpAgain != nullptr ? "yes (saved before delete)" : "no");
    CHECK(cpAgain != nullptr);
    if (cpAgain != nullptr) {
        const int retook = WaitFor(g, 10, [&] { return !cpAgain->IsAlive(); });
        std::printf("  it triggers again %d frames after the reload\n", retook);
        CHECK(retook >= 0);
        CHECK(WaitForHud(g, "Checkpoint...", 3));
    }
    g.Steps(60);
    CHECK_EQ(Script::g_lives, livesBeforeDeath - 1);

    std::printf("-- he still walks and jumps after the reload\n");
    const ETHEntity w = WaitForPlayerReady(g, 120, "after checkpoint");
    CHECK(Ready(w));
    if (!Ready(w)) return;
    // animateCharacter (controlCharacters.as:222-268): walking left cycles
    // frames 4-7, right 8-11, a frame every stride (100 ms) and a bit; standing
    // keeps the facing's first frame. The ORIGINAL restarts the cycle where
    // two floor tiles meet: doCharacterCollision keeps only the LAST collided
    // box's thinner-box test (controlCharacters.as:126, `thinnerBoxHit =`), so
    // for one frame at a seam the wizard is "not touching the ground".
    const float x0 = w->GetPosition().x;
    std::vector<float> seamFrames;
    std::set<uint> leftFrames;
    std::set<uint> rightFrames;
    for (int i = 0; i < 40; ++i) {
        g.Step(g.With({K_LEFT}));
        leftFrames.insert(w->GetFrame());
        if (w->GetUIntData("touchingGround") == 0) seamFrames.push_back(w->GetPosition().x);
    }
    g.Step();
    const uint standLeft = w->GetFrame();
    const float x1 = w->GetPosition().x;
    for (int i = 0; i < 40; ++i) {
        g.Step(g.With({K_RIGHT}));
        rightFrames.insert(w->GetFrame());
        if (w->GetUIntData("touchingGround") == 0) seamFrames.push_back(w->GetPosition().x);
    }
    g.Step();
    const uint standRight = w->GetFrame();
    const float x2 = w->GetPosition().x;
    std::printf("  frames walking left {");
    for (const uint f : leftFrames) std::printf(" %u", f);
    std::printf(" } then standing %u; walking right {", standLeft);
    for (const uint f : rightFrames) std::printf(" %u", f);
    std::printf(" } then standing %u\n", standRight);
    std::printf("  touchingGround 0 while walking at x");
    for (const float x : seamFrames) std::printf(" %.1f", x);
    std::printf(" (floor01 176 and 177 meet at x 3328)\n");
    // E11 (Script.hpp kFloorSeamFix): no walking frame on flat floor is airborne.
    CHECK_MSG(!Penumbra::Script::kFloorSeamFix || seamFrames.empty(), "E11: the wizard turned airborne at a floor seam");
    CHECK(leftFrames.size() >= 3u && *leftFrames.begin() == 4u && *leftFrames.rbegin() <= 7u);
    CHECK(rightFrames.size() >= 3u && *rightFrames.begin() == 8u && *rightFrames.rbegin() <= 11u);
    CHECK_EQ(standLeft, 4u);
    CHECK_EQ(standRight, 8u);
    std::printf("  x %.1f -LEFT 40-> %.1f -RIGHT 40-> %.1f (100 px each way at 150 px/s, unless a wall)\n", x0, x1, x2);
    CHECK(std::fabs(x1 - x0) > 20.0f || std::fabs(x2 - x1) > 20.0f);
    WaitForPlayerReady(g, 60, "before the jump");
    const float y0 = w->GetPosition().y;
    g.Step(g.With({K_UP}));
    float apex = y0;
    for (int i = 0; i < 40; ++i) {
        g.Step();
        apex = std::min(apex, w->GetPosition().y);
    }
    std::printf("  jump: y %.1f, apex %.1f (%.1f px up)\n", y0, apex, y0 - apex);
    CHECK(y0 - apex > 30.0f);
}

// === 5. Level transition =========================================================================

void ScenarioNextLevel(Game& g) {
    ETHEntity p = Player();
    if (!Ready(p)) p = WaitForPlayerReady(g, 240, "next level");
    if (GetSceneFileName() != "scenes/checkpoint.esc" && GetSceneFileName() != "scenes/level1.esc") {
        p = EnsureLevel(g, "scenes/level1.esc");
    }
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard");
        return;
    }
    std::printf("  from %s\n", GetSceneFileName().c_str());
    // level1's next_level id 748 at (2136,-1991), name=level2.esc.
    const ETHEntity door = SeekEntity(748);
    CHECK(door != nullptr);
    if (door == nullptr) return;
    CHECK(door->GetEntityName() == "next_level");
    const string target = door->GetStringData("name");
    std::printf("  next_level's 'name' = '%s'\n", target.c_str());
    CHECK(target == "level2.esc");

    // SHORTCUT: the wizard is teleported onto the exit; then DOWN is held.
    std::printf("  SHORTCUT: wizard teleported onto next_level 748; DOWN held\n");
    Teleport(p, door->GetPositionXY());
    const InputFrame down = g.With({K_DOWN});
    const int fading = WaitFor(g, 10, [&] { return door->CheckCustomData("fadeOut") != DT_NODATA; }, &down);
    const float dist = Dist(p, door);
    std::printf("  fade started %d frames after the teleport, the wizard %.1f px from the exit (< 80)\n", fading,
                dist);
    CHECK(fading >= 0);
    CHECK(dist < 80.0f);
    bool loading = false;
    const int loaded = WaitFor(g, 200, [&] {
        loading = loading || HudHas(g.m, "Carregando...");
        return GetSceneFileName() == "scenes/" + target;
    }, &down);
    std::printf("  %s loaded %d frames after the fade began; 'Carregando...' %s\n", ("scenes/" + target).c_str(),
                loaded, loading ? "shown" : "NOT shown");
    CHECK(loaded >= 180 && loaded <= 182);
    CHECK(loading);
    const ETHEntity w = WaitForPlayerReady(g, 240, "level2 start");
    CHECK(Ready(w));
    if (w != nullptr) {
        std::printf("  level2: wizard at (%.1f,%.1f) (marker 26 at (173,460)), hp %d\n", w->GetPosition().x,
                    w->GetPosition().y, w->GetIntData("hp"));
        CHECK(std::fabs(w->GetPosition().x - 173.0f) < 2.0f);
        CHECK(w->CheckCustomData("hasCheckpoint") == DT_NODATA);
    }
    // level2's 'play' 577 at (205,492) starts the level's music and goes
    // (setupScene.as:43-50).
    std::printf("  fase.mp3 looping %s; play 577 %s\n", g.sound.Looping("fase.mp3") ? "yes" : "NO",
                SeekEntity(577) == nullptr ? "gone" : "STILL THERE");
    CHECK(g.sound.Looping("fase.mp3"));
    CHECK(SeekEntity(577) == nullptr);
}

// === 2. Ranged enemy and fire resistance ==============================================================

void ScenarioImpy(Game& g) {
    const ETHEntity p = EnsureLevel(g, "scenes/level2.esc");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard");
        return;
    }
    // SHORTCUT: impy marker 481 (890,-1439) moved 150 px to the wizard's right.
    const ETHEntity marker = FindMarker("impy", 481);
    CHECK(marker != nullptr);
    if (marker == nullptr) return;
    const vector2 wiz = p->GetPositionXY();
    std::printf("  SHORTCUT: impy marker %d moved to (%.0f,%.0f)\n", marker->GetID(), wiz.x + 150.0f, wiz.y - 20.0f);
    const int since = GetLastID();
    marker->SetPosition(vector3(wiz.x + 150.0f, wiz.y - 20.0f, 0.0f));
    ETHEntity impy;
    WaitFor(g, 3, [&] {
        const ETHEntityArray found = NewEntities("impy.ent", since);
        if (!found.empty()) impy = found[0];
        return impy != nullptr;
    });
    CHECK(impy != nullptr);
    if (impy == nullptr) return;
    CHECK_EQ(impy->GetIntData("hp"), 75);
    CHECK_EQ(impy->GetIntData("damage"), 18);
    CHECK(impy->CheckCustomData("fireResistant") == DT_UINT);
    CHECK_EQ(impy->GetUIntData("fireResistant"), 1u);
    CHECK_EQ(impy->GetUIntData("coolDown"), 1500u);
    WaitFor(g, 60, [&] { return impy->GetUIntData("touchingGround") != 0; });
    std::printf("  impy id %d at (%.1f,%.1f), wizard at (%.1f,%.1f): dx %.1f dy %.1f\n", impy->GetID(),
                impy->GetPosition().x, impy->GetPosition().y, p->GetPosition().x, p->GetPosition().y,
                impy->GetPosition().x - p->GetPosition().x, impy->GetPosition().y - p->GetPosition().y);

    std::printf("-- the wizard's fireball (D) on a fire-resistant impy: damage / 5, hit_fail\n");
    if (p->GetUIntData("currentDir") != Script::RIGHT) {
        g.Step(g.With({K_RIGHT}));
        g.Steps(3);
    }
    CHECK_EQ(p->GetUIntData("currentDir"), Script::RIGHT);
    const int mp0 = p->GetIntData("mp");
    const int impyHp0 = impy->GetIntData("hp");
    const int since2 = GetLastID();
    Spotter spot(since2, {"fire_ball.ent", "hit_fail.ent", "explosion.ent"});
    g.Step(g.With({K_D}));
    spot.Poll();
    const ETHEntity ball = spot.First("fire_ball.ent");
    CHECK(ball != nullptr);
    if (ball != nullptr) {
        CHECK_EQ(ball->GetIntData("ownerID"), p->GetID());
        CHECK_EQ(ball->GetIntData("damage"), 30);
    }
    // castSpell's 10 mana; doMpRecovery may give 1 back on the same frame.
    const int spent = mp0 - p->GetIntData("mp");
    std::printf("  mp %d -> %d\n", mp0, p->GetIntData("mp"));
    CHECK(spent == 10 || spent == 9);
    const int hit = WaitFor(g, 90, [&] {
        spot.Poll();
        return impy->GetIntData("hp") != impyHp0;
    });
    spot.Poll();
    const int expected = MainCharDamage(30, Script::g_charLevel[0]) / 5;
    std::printf("  impy hp %d -> %d after %d frames (expected -%d = (30 + int(30*%d/4)) / 5); hit_fail.ent %u, "
                "explosion.ent %u; fireball alive %s\n",
                impyHp0, impy->GetIntData("hp"), hit, expected, Script::g_charLevel[0], spot.Count("hit_fail.ent"),
                spot.Count("explosion.ent"), (ball != nullptr && ball->IsAlive()) ? "yes" : "no");
    CHECK(hit > 0);
    CHECK_EQ(impyHp0 - impy->GetIntData("hp"), expected);
    CHECK(spot.Count("hit_fail.ent") >= 1u);
    CHECK_EQ(spot.Count("explosion.ent"), 0u);
    if (ball != nullptr) CHECK(!ball->IsAlive());

    std::printf("-- the impy casts fire_ball at the wizard\n");
    const int hp0 = p->GetIntData("hp");
    const int since3 = GetLastID();
    Spotter spot3(since3, {"fire_ball.ent", "explosion.ent"});
    ETHEntity impyBall;
    const int cast = WaitFor(g, 150, [&] {
        spot3.Poll();
        for (const auto& entry : spot3.seen["fire_ball.ent"]) {
            if (entry.second->GetIntData("ownerID") == impy->GetID()) impyBall = entry.second;
        }
        return impyBall != nullptr;
    });
    const uint castFrame = g.Frame();
    // characterAI.as:110 plays it with the cast (hit_fail.ent's particles play
    // the same sample, so the frame is what identifies this play).
    std::printf("  impy's fire_ball after %d frames; cast_fire_spell.ogg played that frame: %s\n", cast,
                g.sound.PlayedAt("cast_fire_spell.ogg", castFrame) ? "yes" : "NO");
    CHECK(cast >= 0);
    CHECK(g.sound.PlayedAt("cast_fire_spell.ogg", castFrame));
    if (impyBall == nullptr) return;
    CHECK_EQ(impyBall->GetIntData("damage"), 18);
    int wizardHp = hp0;
    const int reached = WaitFor(g, 150, [&] {
        spot3.Poll();
        wizardHp = p->GetIntData("hp");
        return wizardHp < hp0;
    });
    spot3.Poll();
    std::printf("  wizard hp %d -> %d after %d frames (expected -18: the impy's damage, not a main character); "
                "explosion.ent %u\n",
                hp0, wizardHp, reached, spot3.Count("explosion.ent"));
    CHECK(reached > 0);
    CHECK_EQ(hp0 - wizardHp, 18);
    CHECK(spot3.Count("explosion.ent") >= 1u);
    CHECK(!impyBall->IsAlive());
}

// === 4. Deaths without a checkpoint, and game over ===============================================

void ScenarioGameOver(Game& g) {
    // A fresh level2 (the impy of scenario 2 is still shooting): the scene
    // loaded as a death reloads it, without the death.
    ETHEntity p = LoadLevel(g, "scenes/level2.esc");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard");
        return;
    }
    CHECK(p->CheckCustomData("hasCheckpoint") == DT_NODATA);

    std::printf("-- a minion sees him: it chases with its chaseSfx (minion.ogg)\n");
    // SHORTCUT: minion marker 200 (-385,-26) moved 250 px to the wizard's right.
    const ETHEntity minionMarker = FindMarker("minion", 200);
    CHECK(minionMarker != nullptr);
    if (minionMarker != nullptr) {
        const vector2 wiz = p->GetPositionXY();
        std::printf("  SHORTCUT: minion marker %d moved to (%.0f,%.0f)\n", minionMarker->GetID(), wiz.x + 250.0f,
                    wiz.y - 20.0f);
        const int since = GetLastID();
        const uint moved = g.Frame();
        minionMarker->SetPosition(vector3(wiz.x + 250.0f, wiz.y - 20.0f, 0.0f));
        ETHEntity minion;
        WaitFor(g, 3, [&] {
            const ETHEntityArray found = NewEntities("minion.ent", since);
            if (!found.empty()) minion = found[0];
            return minion != nullptr;
        });
        CHECK(minion != nullptr);
        if (minion != nullptr) {
            CHECK(minion->GetStringData("chaseSfx") == "soundfx/minion.ogg");
            const int chasing = WaitFor(g, 30, [&] { return minion->GetUIntData("action") == Script::CHASING; });
            const uint chaseFrame = g.Frame();
            g.Steps(20);
            std::printf("  minion id %d chasing after %d frames; minion.ogg played on that frame %s; x %.1f "
                        "(the wizard at %.1f)\n",
                        minion->GetID(), chasing, g.sound.PlayedAt("minion.ogg", chaseFrame) ? "yes" : "NO",
                        minion->GetPosition().x, p->GetPosition().x);
            CHECK(chasing >= 0);
            // meleeCharacterAI (characterAI.as:151-157) plays it as it switches.
            CHECK(g.sound.PlayedSince("minion.ogg", moved));
            CHECK(g.sound.PlayedAt("minion.ogg", chaseFrame));
            CHECK(minion->GetPosition().x < wiz.x + 250.0f - 20.0f);
        }
    }

    std::printf("-- lives run out\n");
    std::printf("  lives %d; every death: SHORTCUT hp set to 0, then the game's fade and reload\n", Script::g_lives);
    int firstCount = -1;
    uint firstSpawns = 0;
    int deaths = 0;
    bool reachedGameOver = false;
    while (deaths < 20) {
        p = WaitForPlayerReady(g, 240, "respawn");
        if (!Ready(p)) {
            CHECK_MSG(false, "the wizard did not respawn");
            break;
        }
        const int lives = Script::g_lives;
        const uint start = Script::g_levelStartTime;
        p->AddIntData("hp", 0);
        ++deaths;
        const int after = WaitFor(g, 200, [&] {
            return Script::g_levelStartTime != start || GetSceneFileName() != "scenes/level2.esc";
        });
        const int count = static_cast<int>(GetNumEntities());
        std::printf("  death %2d: lives %2d -> %2d, %s after %d frames, %d entities, %u markers\n", deaths, lives,
                    Script::g_lives, GetSceneFileName().c_str(), after, count, Script::g_spawn.size());
        CHECK_EQ(Script::g_lives, lives - 1);
        if (GetSceneFileName() == "scenes/gameover.esc") {
            CHECK_EQ(lives, 0);
            CHECK(after >= 181 && after <= 183);
            reachedGameOver = true;
            break;
        }
        CHECK(after >= 181 && after <= 183);
        CHECK(GetSceneFileName() == "scenes/level2.esc");
        if (firstCount < 0) {
            firstCount = count;
            firstSpawns = Script::g_spawn.size();
        } else {
            CHECK_EQ(count, firstCount);
            CHECK_EQ(Script::g_spawn.size(), firstSpawns);
        }
        const ETHEntity w = Player();
        if (w != nullptr) CHECK(w->CheckCustomData("hasCheckpoint") == DT_NODATA);
    }
    CHECK(reachedGameOver);
    if (!reachedGameOver) return;
    CHECK_EQ(Script::g_lives, -1);
    const uint loadedAt = g.Frame();
    std::printf("-- the game-over screen\n");
    CHECK(SeekEntity("bruxo_dead.ent") != nullptr);
    CHECK(g.sound.PlayedSince("laugh_king.mp3", loadedAt));
    CHECK(g.sound.PlayedSince("gameover.mp3", loadedAt));
    g.Steps(3);
    CHECK(HudHasSprite(g.m, "gameover.png"));
    g.Steps(120);
    CHECK(GetSceneFileName() == "scenes/gameover.esc");
    Trace(g, "game over");
    // gameover.as:69: waitForInputToMenu - the cancel button (ESC) goes back.
    g.Step(g.With({K_ESC}));
    const int menu = WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/menu.esc"; });
    std::printf("  ESC: menu.esc %d frames later\n", menu);
    CHECK(menu >= 0);
    g.Steps(5);
    CHECK(SeekEntity("cursor.ent") != nullptr);
}

// === 6b + 8. level3 by the K_3 cheat: the falling bridge, then the king ===========================

void ScenarioBridgeAndBoss(Game& g) {
    CHECK(EnsureMenu(g));
    std::printf("-- New game with 3 held (main.as:112-113): level3\n");
    g.base.cursor = kNewGameButton;
    g.Steps(3);
    CHECK(LastButton() == "novo_jogo");
    const uint gameStart = g.Frame() + 1;
    g.Step(g.With({K_RETURN, K_3}));
    const InputFrame hold3 = g.With({K_3});
    const int loaded = WaitFor(g, 200, [] { return GetSceneFileName() == "scenes/level3.esc"; }, &hold3);
    std::printf("  level3.esc loaded %d frames after the press\n", loaded);
    CHECK(loaded >= 180 && loaded <= 183);
    CHECK_EQ(Script::g_lives, 13);
    CHECK_EQ(Script::g_exp[0], 0);
    const ETHEntity p = WaitForPlayerReady(g, 240, "level3 start");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard in level3");
        return;
    }
    Trace(g, "level3 running");

    std::printf("-- a falling bridge\n");
    // level3's falling_bridge id 1021 at (2592,2576), over lava.
    const ETHEntity bridge = SeekEntity(1021);
    CHECK(bridge != nullptr);
    if (bridge == nullptr) return;
    CHECK(bridge->GetEntityName() == "falling_bridge.ent");
    CHECK(bridge->Collidable());
    const vector3 bridgeStart = bridge->GetPosition();
    // SHORTCUT: the wizard is teleported just above the bridge and lands on it.
    std::printf("  SHORTCUT: wizard teleported 40 px above bridge 1021\n");
    const int since = GetLastID();
    Teleport(p, bridge->GetPositionXY() + vector2(0.0f, -40.0f));
    const int touched = WaitFor(g, 60, [&] { return bridge->CheckCustomData("falling") != DT_NODATA; });
    const uint touchFrame = g.Frame();
    std::printf("  touched after %d frames (falling = %u ms), wizard on the ground %u\n", touched,
                bridge->GetUIntData("falling"), p->GetUIntData("touchingGround"));
    CHECK(touched >= 0);
    const int released = WaitFor(g, 60, [&] { return !bridge->Collidable(); });
    std::printf("  collision off %u frames after the touch (events.as:92: GetTime()-falling > 600)\n",
                g.Frame() - touchFrame);
    CHECK(released >= 0);
    CHECK_EQ(g.Frame() - touchFrame, 37u);
    CHECK(!NewEntities("bridge_fall.ent", since).empty());
    g.Steps(10);
    std::printf("  bridge y %.1f -> %.1f, gravity %.1f\n", bridgeStart.y, bridge->GetPosition().y,
                bridge->GetFloatData("gravity"));
    CHECK(bridge->GetPosition().y > bridgeStart.y + 5.0f);
    CHECK(bridge->GetFloatData("gravity") > 0.0f);

    std::printf("-- event01: the king\n");
    // SHORTCUT: the wizard is teleported into event01's bucket (10920,1408)
    // before he follows the bridge into the lava.
    std::printf("  SHORTCUT: wizard teleported into event01 882's bucket\n");
    const ETHEntity ev = SeekEntity(882);
    CHECK(ev != nullptr);
    if (ev == nullptr) return;
    CHECK(ev->GetEntityName() == "event01");
    const int since2 = GetLastID();
    const uint eventStart = g.Frame();
    Teleport(p, vector2(10920.0f, 1440.0f));
    int bridgeGoneAfter = -1;
    const int fired = WaitFor(g, 10, [&] {
        if (bridgeGoneAfter < 0 && !bridge->IsAlive()) bridgeGoneAfter = static_cast<int>(g.Frame() - eventStart);
        return !ev->IsAlive();
    });
    g.Steps(2);
    if (bridgeGoneAfter < 0 && !bridge->IsAlive()) bridgeGoneAfter = static_cast<int>(g.Frame() - eventStart);
    std::printf("  the bridge, off screen, deleted %d frames after the teleport\n", bridgeGoneAfter);
    CHECK(!bridge->IsAlive());
    const ETHEntityArray kings = NewEntities("king.ent", since2);
    const ETHEntityArray walls = NewEntities("invisible_wall.ent", since2);
    std::printf("  event01 fired %d frames after; king.ent new %u, invisible_wall.ent new %u, summon.ent new %u\n",
                fired, kings.size(), walls.size(), static_cast<uint>(NewEntities("summon.ent", since2).size()));
    CHECK(fired >= 0);
    CHECK_EQ(kings.size(), 1u);
    CHECK_EQ(walls.size(), 1u);
    CHECK(!NewEntities("summon.ent", since2).empty());
    if (!walls.empty()) {
        CHECK_NEAR(walls[0]->GetPosition().x, 10112.0f);
        CHECK_NEAR(walls[0]->GetPosition().y, 1408.0f);
    }
    CHECK(g.sound.PlayedSince("laugh_king.mp3", eventStart));
    CHECK(g.sound.Looping("chefao.mp3"));
    CHECK(g.m.Samples().IsSamplePlaying("soundfx/chefao.mp3"));
    CHECK(!g.m.Samples().IsSamplePlaying("soundfx/fase.mp3"));
    if (kings.empty()) return;
    const ETHEntity king = kings[0];
    std::printf("  king id %d hp %d expGiven %d\n", king->GetID(), king->GetIntData("hp"),
                king->GetIntData("expGiven"));
    CHECK_EQ(king->GetIntData("hp"), 3500);

    std::printf("-- the king fights: paladin_sword up close, fire_ball from afar, warriors summoned every 5 s\n");
    // SHORTCUT: the wizard's hp and maxHp set to 1000 so that he lives through
    // the observation (the king hits for 10, his warriors for 5; addToHp
    // clamps at maxHp).
    std::printf("  SHORTCUT: wizard hp and maxHp set to 1000 for 330 frames\n");
    p->AddIntData("maxHp", 1000);
    p->AddIntData("hp", 1000);
    const int since3 = GetLastID();
    Spotter kingSpot(since3, {"paladin_sword.ent", "fire_ball.ent", "warrior.ent", "summon.ent"});
    std::map<int, int> drops;
    int hpNow = p->GetIntData("hp");
    uint kingSwords = 0;
    uint kingBalls = 0;
    for (uint i = 0; i < 330; ++i) {
        g.Step();
        kingSpot.Poll();
        const int hp = p->GetIntData("hp");
        if (hp < hpNow) ++drops[hpNow - hp];
        hpNow = hp;
    }
    for (const auto& entry : kingSpot.seen["paladin_sword.ent"]) {
        if (entry.second->GetIntData("ownerID") == king->GetID()) ++kingSwords;
    }
    for (const auto& entry : kingSpot.seen["fire_ball.ent"]) {
        if (entry.second->GetIntData("ownerID") == king->GetID()) ++kingBalls;
    }
    std::printf("  in 330 frames: king paladin_sword %u, king fire_ball %u, warriors summoned %u (summon.ent %u); "
                "wizard hp drops:",
                kingSwords, kingBalls, kingSpot.Count("warrior.ent"), kingSpot.Count("summon.ent"));
    for (const auto& entry : drops) std::printf(" -%d x%d", entry.first, entry.second);
    std::printf("; king at (%.1f,%.1f) hp %d\n", king->GetPosition().x, king->GetPosition().y,
                king->GetIntData("hp"));
    CHECK(kingSwords + kingBalls >= 1u);
    // controlCharacters.as:666: summoner(king, "warrior", "summon", 5000).
    CHECK(kingSpot.Count("warrior.ent") >= 1u);
    const ETHEntity summoned = kingSpot.First("warrior.ent");
    if (summoned != nullptr) CHECK_EQ(summoned->GetIntData("expGiven"), 75);
    // The king's blows are his damage 10 (not a main character: 10 * int(1));
    // his warriors' 5.
    for (const auto& entry : drops) CHECK(entry.first == 10 || entry.first == 5 || entry.first == 15);
    CHECK(!drops.empty());
    p->AddIntData("maxHp", 100);
    p->AddIntData("hp", 100);
    std::printf("  SHORTCUT: wizard hp and maxHp back to 100\n");
    g.Steps(5);

    std::printf("-- the king falls: the end screen and the record\n");
    const std::filesystem::path userHs = std::filesystem::path(g.userRoot) / "hs.enml";
    const std::filesystem::path originalHs = std::filesystem::path(PENUMBRA_ORIGINAL_DIR) / "hs.enml";
    const string originalBefore = ReadFile(originalHs);
    CHECK(!FileExists(userHs));
    int e0 = Script::g_exp[0], l0 = Script::g_charLevel[0];
    ExpectedExp(e0, l0, king->GetIntData("expGiven"));
    // SHORTCUT: the king's hp set to 0 (3500 hp is minutes of sword work).
    std::printf("  SHORTCUT: king hp set to 0\n");
    king->AddIntData("hp", 0);
    const int finished = WaitFor(g, 5, [] { return Script::g_gameFinished; });
    const int recorded = WaitFor(g, 5, [] { return Script::g_newRecordTime != 0; });
    const uint record = Script::g_newRecordTime;
    const uint runMs = (g.Frame() - gameStart) * 1000u / 60u;
    std::printf("  g_gameFinished after %d frames, g_newRecordTime %u ms (%s) after %d more; the run since the "
                "press %u ms\n",
                finished, record, Script::getTimeString(record).c_str(), recorded, runMs);
    CHECK(finished >= 0);
    CHECK(recorded >= 0);
    CHECK(record > 0u && record <= runMs + 20u && record + 3200u >= runMs);
    std::printf("  exp player0 %d level %d (expected %d level %d)\n", Script::g_exp[0], Script::g_charLevel[0], e0, l0);
    CHECK_EQ(Script::g_exp[0], e0);
    CHECK_EQ(Script::g_charLevel[0], l0);
    CHECK(!g.m.Samples().IsSamplePlaying("soundfx/chefao.mp3"));
    CHECK(g.sound.PlayedSince("death_king.ogg", eventStart));
    CHECK(FileExists(userHs));
    CHECK(ReadFile(originalHs) == originalBefore);
    enmlFile hs;
    const uint parseError = hs.parseString(GetStringFromFile(userHs.generic_string()));
    CHECK_EQ(parseError, 0u);
    std::printf("  %s:", userHs.generic_string().c_str());
    uint previous = 0;
    for (uint t = 0; t < Script::MAX_SCORES; ++t) {
        uint v = 0;
        const bool has = hs.getUint("hs", "hs" + std::to_string(t), v);
        std::printf(" hs%u=%u", t, v);
        CHECK(has);
        CHECK(v >= previous);
        previous = v;
        CHECK_EQ(v, t == 0 ? record : 3599000u);
    }
    std::printf("\n");
    CHECK(hs.get("hs", "hs5").empty());
    CHECK(WaitForHud(g, "Seu tempo total foi:", 3));
    CHECK(HudHas(g.m, Script::getTimeString(record)));
    CHECK(HudHas(g.m, "Melhores tempos:"));
    CHECK_EQ(p->GetIntData("hp"), 100);
    g.Steps(200);
    CHECK(!king->IsAlive());
    CHECK(GetSceneFileName() == "scenes/level3.esc");
    CHECK(HudHas(g.m, "Seu tempo total foi:"));
    CHECK_EQ(Script::g_newRecordTime, record);

    std::printf("-- ESC, and the menu's best times list the record\n");
    CHECK(EnsureMenu(g));
    g.base.cursor = kRecordsButton;
    g.Steps(3);
    std::printf("  cursor over '%s'\n", LastButton().c_str());
    CHECK(LastButton() == "melhores_tempos");
    CHECK(WaitForHud(g, "1    " + Script::getTimeString(record), 3));
    CHECK_EQ(Script::getGetBestTime(), record);
}

// === 9. Versus ======================================================================================

void ScenarioPvp(Game& g) {
    CHECK(EnsureMenu(g));
    std::printf("-- a second controller: pad 0 plugged (player 2 uses it, g_controls = 0)\n");
    g.base.pads[0].connected = true;
    g.base.cursor = kVersusButton;
    g.Steps(3);
    std::printf("  cursor over '%s'; hasASecondController %s\n", LastButton().c_str(),
                Script::hasASecondController() ? "yes" : "NO");
    CHECK(Script::hasASecondController());
    CHECK(LastButton() == "versus");
    CHECK(WaitForHud(g, "Escolha uma arena", 3));
    g.Step(g.With({K_RETURN}));
    const int select = WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/arena_select.esc"; });
    std::printf("  arena_select.esc %d frames after Enter; lives %d\n", select, Script::g_lives);
    CHECK(select >= 0);
    CHECK_EQ(Script::g_lives, 13);
    g.Steps(3);
    // menu.as:362-372: an arena with a 'score' stays dark until a campaign ends
    // faster; the record set by the king unlocked Neblina (900000 ms).
    const ETHEntity neblina = SeekEntity(45);
    if (neblina != nullptr) {
        std::printf("  Neblina (score 900000) colour %.2f (best time %u)\n", neblina->GetColor().x,
                    Script::getGetBestTime());
        CHECK((neblina->GetColor().x < 0.5f) == (Script::getGetBestTime() >= 900000u));
    }
    g.base.cursor = kArena1Thumbnail;
    g.Steps(3);
    std::printf("  cursor over '%s'\n", LastButton().c_str());
    CHECK(LastButton() == "thumbnail");
    CHECK(WaitForHud(g, "Obelisco", 3));
    g.Step(g.With({K_RETURN}));
    const ETHEntity cursor = SeekEntity("cursor.ent");
    if (cursor != nullptr) CHECK(cursor->GetStringData("scene") == "pvp_lv1.esc");
    const int loaded = WaitFor(g, 200, [] { return GetSceneFileName() == "scenes/pvp_lv1.esc"; });
    std::printf("  pvp_lv1.esc %d frames after Enter\n", loaded);
    CHECK(loaded >= 180 && loaded <= 183);

    const int both = WaitFor(g, 240, [] { return Ready(Player()) && Ready(Princess()); });
    CHECK(both >= 0);
    if (both < 0) return;
    const ETHEntity w0 = Player();
    const ETHEntity q0 = Princess();
    std::printf("  wizard hp %d/%d pvpMode %u; princess hp %d/%d pvpMode %u\n", w0->GetIntData("hp"),
                w0->GetIntData("maxHp"), w0->GetUIntData("pvpMode"), q0->GetIntData("hp"), q0->GetIntData("maxHp"),
                q0->GetUIntData("pvpMode"));
    CHECK_EQ(w0->GetIntData("hp"), 500);
    CHECK_EQ(w0->GetIntData("maxHp"), 500);
    CHECK_EQ(q0->GetIntData("hp"), 500);
    CHECK_EQ(q0->GetIntData("maxHp"), 500);
    CHECK_EQ(w0->GetUIntData("pvpMode"), 1u);
    CHECK_EQ(q0->GetUIntData("pvpMode"), 1u);
    CHECK(g.sound.Looping("chefao.mp3"));

    const uint matchStart = g.Frame();
    for (int round = 1; round <= Script::MAX_PVP_POINTS; ++round) {
        const int ready = WaitFor(g, 240, [] { return Ready(Player()) && Ready(Princess()); });
        CHECK(ready >= 0);
        if (ready < 0) return;
        const ETHEntity w = Player();
        const ETHEntity q = Princess();
        const int points = Script::g_pvpPoints[0];
        // SHORTCUT: the princess is teleported 30 px to the wizard's right and
        // her hp set to 20 (a sword blow is 25).
        Teleport(q, w->GetPositionXY() + vector2(30.0f, -4.0f));
        g.Steps(10);
        q->AddIntData("hp", 20);
        if (w->GetUIntData("currentDir") != Script::RIGHT) {
            g.Step(g.With({K_RIGHT}));
            g.Steps(15);
        }
        uint lastPress = 0;
        int scored = -1;
        for (int i = 0; i < 180; ++i) {
            if (Script::g_pvpPoints[0] > points) {
                scored = i;
                break;
            }
            const float dx = q->GetPosition().x - w->GetPosition().x;
            if (dx > 0.0f && dx < 50.0f && g.Frame() >= lastPress + 15) {
                lastPress = g.Frame();
                g.Step(g.With({K_S}));
            } else {
                g.Step();
            }
        }
        std::printf("  round %d: SHORTCUT princess beside the wizard with hp 20; point after %d frames: %d - %d\n",
                    round, scored, Script::g_pvpPoints[0], Script::g_pvpPoints[1]);
        CHECK(scored >= 0);
        CHECK_EQ(Script::g_pvpPoints[0], points + 1);
        if (scored < 0) return;
        if (round < Script::MAX_PVP_POINTS) {
            const int reload = WaitForSetup(g, 200);
            std::printf("    %s set up again %d frames later\n", GetSceneFileName().c_str(), reload);
            CHECK(reload >= 180 && reload <= 184);
            CHECK(GetSceneFileName() == "scenes/pvp_lv1.esc");
        }
    }
    const int won = WaitFor(g, 3, [] { return Script::g_gameFinished; });
    std::printf("  3 points: g_gameFinished after %d frames; pvp_win.ogg %s\n", won,
                g.sound.PlayedSince("pvp_win.ogg", matchStart) ? "played" : "NOT played");
    CHECK(won >= 0);
    CHECK(g.sound.PlayedSince("pvp_win.ogg", matchStart));
    CHECK(WaitForHud(g, "Jogador 1 \xE9 o vencedor!", 3));
    CHECK(HudHasSprite(g.m, "gameover.png"));
    const uint setupAt = Script::g_levelStartTime;
    g.Steps(240);
    std::printf("  240 frames later: still %s, no reload %s\n", GetSceneFileName().c_str(),
                Script::g_levelStartTime == setupAt ? "yes" : "NO");
    CHECK(Script::g_levelStartTime == setupAt);
    CHECK(HudHas(g.m, "vencedor"));
    CHECK(EnsureMenu(g));
    g.base.pads[0].connected = false;
}

// === 11. The wizard's other moves: double jump, light, the two combos; a knight ======================

void ScenarioMoves(Game& g) {
    std::printf("  (a fresh campaign: newGame(\"CAMPAIGN\"), as the menu calls it)\n");
    const uint setup = Script::g_levelStartTime;
    Script::newGame("CAMPAIGN");
    WaitFor(g, 3, [&] { return Script::g_levelStartTime != setup; });
    CHECK(GetSceneFileName() == "scenes/level1.esc");
    const ETHEntity p = WaitForPlayerReady(g, 240, "moves");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard");
        return;
    }
    g.Steps(20);

    std::printf("-- double jump: the second costs 4 mana, a third does nothing\n");
    const int mp0 = p->GetIntData("mp");
    g.Step(g.With({K_UP}));
    CHECK_EQ(p->GetIntData("jumps"), 1);
    g.Steps(12);
    CHECK_EQ(p->GetUIntData("touchingGround"), 0u);
    const uint second = g.Frame() + 1;
    g.Step(g.With({K_UP}));
    const int mp1 = p->GetIntData("mp");
    std::printf("  jumps %d, mp %d -> %d, jump01.ogg %s\n", p->GetIntData("jumps"), mp0, mp1,
                g.sound.PlayedAt("jump01.ogg", second) ? "played" : "NOT played");
    CHECK_EQ(p->GetIntData("jumps"), 2);
    CHECK(mp0 - mp1 == 4 || mp0 - mp1 == 3);
    CHECK(g.sound.PlayedAt("jump01.ogg", second));
    g.Steps(4);
    const float vy = p->GetFloatData("forceY");
    g.Step(g.With({K_UP}));
    CHECK_EQ(p->GetIntData("jumps"), 2);
    CHECK(p->GetFloatData("forceY") >= vy);
    const int landed = WaitFor(g, 120, [&] { return p->GetUIntData("touchingGround") != 0; });
    CHECK(landed >= 0);
    g.Step();
    CHECK_EQ(p->GetIntData("jumps"), 0);

    std::printf("-- the light spell (SPACE): 50 mana, follows him, one at a time\n");
    // SHORTCUT: mana restored to the maximum.
    p->AddIntData("mp", p->GetIntData("maxMp"));
    std::printf("  SHORTCUT: mp set to maxMp\n");
    g.Steps(2);
    const int mpL = p->GetIntData("mp");
    const int sinceL = GetLastID();
    const uint cast = g.Frame() + 1;
    g.Step(g.With({K_SPACE}));
    const ETHEntityArray lights = NewEntities("light_spell.ent", sinceL);
    CHECK_EQ(lights.size(), 1u);
    CHECK(mpL - p->GetIntData("mp") == 50 || mpL - p->GetIntData("mp") == 49);
    CHECK(g.sound.PlayedAt("light_spell.mp3", cast));
    if (!lights.empty()) {
        const ETHEntity light = lights[0];
        g.Steps(2);
        const vector2 off = light->GetPositionXY() - p->GetPositionXY();
        std::printf("  light at wizard %+.1f,%+.1f (spellOffset 0,-40)\n", off.x, off.y);
        CHECK(std::fabs(off.x) < 1.0f && std::fabs(off.y + 40.0f) < 1.0f);
        const int mpBefore = p->GetIntData("mp");
        g.Step(g.With({K_SPACE}));
        CHECK(WaitForHud(g, "em execu", 3));
        CHECK(p->GetIntData("mp") >= mpBefore);
        CHECK_EQ(static_cast<uint>(NewEntities("light_spell.ent", sinceL).size()), 1u);
        g.Steps(20, g.With({K_RIGHT}));
        const vector2 off2 = light->GetPositionXY() - p->GetPositionXY();
        std::printf("  after walking right: light at wizard %+.1f,%+.1f\n", off2.x, off2.y);
        CHECK(std::fabs(off2.x) < 3.0f && std::fabs(off2.y + 40.0f) < 3.0f);
        g.Step();
    }

    std::printf("-- a knight: its blow (10) and its jump back (jumpBackAfterAttack)\n");
    const ETHEntity marker = FindMarker("knight", 676);
    CHECK(marker != nullptr);
    if (marker == nullptr) return;
    g.Steps(10);
    const vector2 wiz = p->GetPositionXY();
    std::printf("  SHORTCUT: knight marker %d moved to (%.0f,%.0f)\n", marker->GetID(), wiz.x + 90.0f, wiz.y - 20.0f);
    const int sinceK = GetLastID();
    marker->SetPosition(vector3(wiz.x + 90.0f, wiz.y - 20.0f, 0.0f));
    ETHEntity knight;
    WaitFor(g, 3, [&] {
        const ETHEntityArray found = NewEntities("knight.ent", sinceK);
        if (!found.empty()) knight = found[0];
        return knight != nullptr;
    });
    CHECK(knight != nullptr);
    if (knight == nullptr) return;
    CHECK_EQ(knight->GetIntData("hp"), 150);
    CHECK_EQ(knight->GetUIntData("jumpBackAfterAttack"), 1u);
    Spotter ks(sinceK, {"enemy_sword.ent"});
    int hpW = p->GetIntData("hp");
    int firstDrop = 0;
    float kbx = 0.0f;
    float kby = 0.0f;
    const int attacked = WaitFor(g, 300, [&] {
        ks.Poll();
        for (const auto& entry : ks.seen["enemy_sword.ent"]) {
            if (entry.second->GetIntData("ownerID") == knight->GetID() && kbx == 0.0f) {
                kbx = knight->GetFloatData("knockBackX");
                kby = knight->GetFloatData("knockBackY");
            }
        }
        const int hp = p->GetIntData("hp");
        if (hp < hpW && firstDrop == 0) firstDrop = hpW - hp;
        hpW = hp;
        return firstDrop != 0 && kbx != 0.0f;
    });
    std::printf("  knight attacked after %d frames: wizard -%d, the knight's knockBack then (%.2f,%.2f) "
                "(characterAI.as:190-194: (12,-12) away from him, after one move's 0.8)\n",
                attacked, firstDrop, kbx, kby);
    CHECK(attacked >= 0);
    CHECK_EQ(firstDrop, 10);
    CHECK(kbx > 0.0f);
    CHECK(kby < 0.0f);

    std::printf("-- the sword combo RIGHT, RIGHT, S: combo_sword, 5 mana, 5x damage\n");
    // SHORTCUT: mana restored to the maximum.
    p->AddIntData("mp", p->GetIntData("maxMp"));
    WaitFor(g, 120, [&] {
        const float dx = knight->GetPosition().x - p->GetPosition().x;
        return dx > 0.0f && dx < 55.0f && knight->GetFloatData("knockBackX") == 0.0f && Ready(p);
    });
    const int khp = knight->GetIntData("hp");
    const int mpC = p->GetIntData("mp");
    const int sinceC = GetLastID();
    const uint comboStart = g.Frame() + 1;
    g.Step(g.With({K_RIGHT}));
    g.Step();
    g.Step(g.With({K_RIGHT}));
    g.Step();
    g.Step(g.With({K_S}));
    const ETHEntityArray comboSwords = NewEntities("combo_sword.ent", sinceC);
    const ETHEntityArray plainSwords = NewEntities("sword0.ent", sinceC);
    g.Steps(3);
    const int comboDamage = MainCharDamage(20, Script::g_charLevel[0]) * 5;
    std::printf("  combo_sword.ent %u, sword0.ent %u; mp %d -> %d; knight hp %d -> %d (expected -%d = (20 + "
                "int(20*%d/4)) * int(5)); sword_combo.ogg %s\n",
                comboSwords.size(), plainSwords.size(), mpC, p->GetIntData("mp"), khp, knight->GetIntData("hp"),
                comboDamage, Script::g_charLevel[0],
                g.sound.PlayedSince("sword_combo.ogg", comboStart) ? "played" : "NOT played");
    CHECK_EQ(comboSwords.size(), 1u);
    CHECK_EQ(plainSwords.size(), 0u);
    CHECK(mpC - p->GetIntData("mp") == 5 || mpC - p->GetIntData("mp") == 4);
    CHECK_EQ(khp - knight->GetIntData("hp"), comboDamage);
    CHECK(g.sound.PlayedSince("sword_combo.ogg", comboStart));

    std::printf("-- the spell combo DOWN, RIGHT, D: combo_fire_ball, 25 mana, 225 base damage\n");
    p->AddIntData("mp", p->GetIntData("maxMp"));
    // SHORTCUT: the knight is put 150 px away, out of its own reach: a
    // fireball also bursts on any collidable blow it meets (doDamage with
    // collideEverything), and the knight's sword is out every 300 ms up close.
    WaitFor(g, 60, [] {
        ETHEntityArray swords;
        GetEntityArray("enemy_sword.ent", swords);
        return swords.empty();
    });
    Teleport(knight, p->GetPositionXY() + vector2(150.0f, -10.0f));
    std::printf("  SHORTCUT: knight teleported 150 px to the wizard's right\n");
    g.Steps(8);
    const int mpS = p->GetIntData("mp");
    const int exp0 = Script::g_exp[0];
    int level0 = Script::g_charLevel[0];
    const int sinceS = GetLastID();
    const uint spellStart = g.Frame() + 1;
    Spotter ss(sinceS, {"combo_fire_ball.ent", "fire_ball.ent", "big_explosion.ent"});
    if (p->GetUIntData("currentDir") != Script::RIGHT) {
        CHECK_MSG(false, "the wizard does not face the knight");
    }
    g.Step(g.With({K_DOWN}));
    g.Step();
    g.Step(g.With({K_RIGHT}));
    g.Step();
    g.Step(g.With({K_D}));
    ss.Poll();
    const int killed = WaitFor(g, 90, [&] {
        ss.Poll();
        return knight->GetIntData("hp") <= 0;
    });
    ss.Poll();
    std::printf("  combo_fire_ball.ent %u, fire_ball.ent %u; mp %d -> %d; knight dead after %d frames; "
                "big_explosion.ent %u; blast_attack.ogg %s\n",
                ss.Count("combo_fire_ball.ent"), ss.Count("fire_ball.ent"), mpS, p->GetIntData("mp"), killed,
                ss.Count("big_explosion.ent"),
                g.sound.PlayedSince("blast_attack.ogg", spellStart) ? "played" : "NOT played");
    CHECK_EQ(ss.Count("combo_fire_ball.ent"), 1u);
    CHECK_EQ(ss.Count("fire_ball.ent"), 0u);
    CHECK(killed >= 0);
    CHECK(ss.Count("big_explosion.ent") >= 1u);
    CHECK(g.sound.PlayedSince("blast_attack.ogg", spellStart));
    const ETHEntity ball = ss.First("combo_fire_ball.ent");
    if (ball != nullptr) CHECK_EQ(ball->GetIntData("damage"), 225);
    g.Steps(2);
    int e0 = exp0;
    ExpectedExp(e0, level0, 150);
    std::printf("  exp %d -> %d (expected %d)\n", exp0, Script::g_exp[0], e0);
    CHECK_EQ(Script::g_exp[0], e0);
}

// === 12. Co-op: the creature summoned by the second controller ==========================================

void ScenarioCoop(Game& g) {
    const ETHEntity p = LoadLevel(g, "scenes/level1.esc");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard");
        return;
    }
    std::printf("-- pad 0 plugged; START (JK_10) on it summons the princess\n");
    g.base.pads[0].connected = true;
    g.Steps(2);
    CHECK(Script::hasASecondController());
    // SHORTCUT: mana restored to the maximum.
    p->AddIntData("mp", p->GetIntData("maxMp"));
    std::printf("  SHORTCUT: mp set to maxMp\n");
    g.Steps(2);
    const int lives = Script::g_lives;
    const int mp0 = p->GetIntData("mp");
    const int since = GetLastID();
    InputFrame start = g.base;
    start.pads[0].buttons[static_cast<std::size_t>(JK_10)] = true;
    g.Step(start);
    const ETHEntityArray princesses = NewEntities("princess.ent", since);
    std::printf("  princess.ent %u, summon.ent %u, mp %d -> %d, lives %d -> %d\n", princesses.size(),
                static_cast<uint>(NewEntities("summon.ent", since).size()), mp0, p->GetIntData("mp"), lives,
                Script::g_lives);
    CHECK_EQ(princesses.size(), 1u);
    CHECK(!NewEntities("summon.ent", since).empty());
    CHECK(mp0 - p->GetIntData("mp") == 50 || mp0 - p->GetIntData("mp") == 49);
    CHECK_EQ(Script::g_lives, lives - 1);
    CHECK(WaitForHud(g, "Criatura m\xE1gica invocada", 3));
    if (princesses.empty()) return;
    const ETHEntity q = princesses[0];
    g.Steps(20);
    std::printf("  princess at (%.1f,%.1f), wizard at (%.1f,%.1f)\n", q->GetPosition().x, q->GetPosition().y,
                p->GetPosition().x, p->GetPosition().y);

    std::printf("-- a second summon is refused\n");
    const int mp1 = p->GetIntData("mp");
    const int since2 = GetLastID();
    g.Step(start);
    CHECK(NewEntities("princess.ent", since2).empty());
    CHECK_EQ(Script::g_lives, lives - 1);
    CHECK(p->GetIntData("mp") >= mp1);
    CHECK(WaitForHud(g, "invocar 2 criaturas", 3));

    std::printf("-- pad 0 moves and arms her\n");
    WaitFor(g, 60, [&] { return q->GetUIntData("touchingGround") != 0; });
    const float qx = q->GetPosition().x;
    InputFrame right = g.base;
    right.pads[0].xy = vector2(1.0f, 0.0f);
    g.Steps(30, right);
    g.Step();
    std::printf("  princess x %.1f -> %.1f in 30 frames (150 px/s: 75), dir %u\n", qx, q->GetPosition().x,
                q->GetUIntData("currentDir"));
    CHECK(q->GetPosition().x - qx > 50.0f);
    CHECK_EQ(q->GetUIntData("currentDir"), Script::RIGHT);
    g.Steps(15);
    const int since3 = GetLastID();
    InputFrame attack = g.base;
    attack.pads[0].buttons[static_cast<std::size_t>(JK_04)] = true;
    g.Step(attack);
    CHECK_EQ(static_cast<uint>(NewEntities("sword1.ent", since3).size()), 1u);
    // The camera frames the two of them (cameraManager.as:118-121).
    const vector2 mid = (p->GetPositionXY() + q->GetPositionXY()) / 2.0f;
    const vector2 onScreen = mid - GetCameraPos();
    std::printf("  midpoint on screen at (%.1f,%.1f)\n", onScreen.x, onScreen.y);
    CHECK(onScreen.x >= 0.39f * 1024.0f && onScreen.x <= 0.61f * 1024.0f);

    std::printf("-- off screen for 3 s she vanishes; it costs no further life and no reload\n");
    // SHORTCUT: the princess is teleported 2000 px away.
    std::printf("  SHORTCUT: princess teleported 2000 px to the right\n");
    const uint setup = Script::g_levelStartTime;
    Teleport(q, q->GetPositionXY() + vector2(2000.0f, 0.0f));
    bool warned = false;
    const int died = WaitFor(g, 240, [&] {
        warned = warned || HudHas(g.m, "n\xE3o pode sair do campo de vis\xE3o");
        return q->GetIntData("hp") <= 0;
    });
    std::printf("  hp 0 after %d frames off screen (vanishTime + 3000 ms); warning shown %s\n", died,
                warned ? "yes" : "NO");
    CHECK(died >= 180 && died <= 190);
    CHECK(warned);
    const int gone = WaitFor(g, 200, [&] { return !q->IsAlive(); });
    std::printf("  deleted %d frames later; lives %d; scene set up again %s\n", gone, Script::g_lives,
                Script::g_levelStartTime != setup ? "YES" : "no");
    CHECK(gone >= 180 && gone <= 183);
    CHECK_EQ(Script::g_lives, lives - 1);
    CHECK(Script::g_levelStartTime == setup);
    CHECK(Ready(Player()));
    g.base.pads[0].connected = false;
    g.Steps(5);
}

// === 14. The options screen's enhanced rows (E10) ===========================================
//
// ENHANCEMENT E10 (game/script/videoModes.cpp): keyboard player 2, the view,
// the language and the two volumes, below the original's g_controls. No layer
// here, so each starts at its constructed value: row 0 of each switch, 100%
// on both steppers. Run just before the Quit rather than in scenario 10: it
// adds some 300 frames, and the runtime reseeds rand() from the clock after
// every frame with particles in view (Machine.cpp, ETHScene.cpp:1129-1130), so
// frames added early in the chain would reshuffle every random roll after them.
void ScenarioOptionsE10(Game& g) {
    CHECK(EnsureMenu(g));
    g.base.cursor = kOptionsButton;
    g.Steps(3);
    std::printf("  cursor over '%s'\n", LastButton().c_str());
    CHECK(LastButton() == "opcoes_de_video");
    g.Step(g.With({K_RETURN}));
    const int optionsAfter = WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/videoModes.esc"; });
    std::printf("  videoModes.esc loaded %d frames after the press\n", optionsAfter);
    CHECK(optionsAfter >= 0);
    g.Steps(3);

    // Each option is a click, as in scenario 10: the cursor over it, then a
    // fresh Enter.
    const auto click = [&g](const vector2& at) {
        g.base.cursor = at;
        g.Steps(2);
        g.Step(g.With({K_RETURN}));
        g.Steps(2);
    };

    CHECK(WaitForHud(g, "[\x95] Teclado para o jogador 2", 3));
    CHECK(HudHas(g.m, "[ ] Jogador 2 s\xF3 no joystick"));
    CHECK(HudHas(g.m, "[\x95] Tela larga (widescreen)"));
    CHECK(HudHas(g.m, "[ ] Tela 4:3 (original)"));
    CHECK(HudHas(g.m, "Vale a partir da pr\xF3xima fase"));
    CHECK(HudHas(g.m, "[\x95] Portugu\xEAs"));
    CHECK(HudHas(g.m, "[ ] English"));
    CHECK(HudHas(g.m, "Volume da m\xFAsica"));
    CHECK(HudHas(g.m, "Volume dos efeitos"));
    // E8's switch (drawn at y 694-744 by the same Switch::put the three rows
    // clicked below exercise; read only, so no later scenario's timing moves).
    CHECK(HudHas(g.m, "[\x95] Ativa movimento suave"));
    CHECK(HudHas(g.m, "[ ] Desativa movimento suave"));
    CHECK(HudHas(g.m, "[\x95] Pausa ao perder o foco"));   // E13's, beside it
    CHECK(HudHas(g.m, "[ ] Continua sem o foco"));
    CHECK(HudHas(g.m, "[<]"));
    CHECK(HudHas(g.m, "[>]"));
    CHECK(HudHas(g.m, "100%"));
    // The original's rows are still drawn where they were.
    CHECK(HudHas(g.m, "[\x95] Ativa pixel shaders"));
    CHECK(HudHas(g.m, "[\x95] Janela"));

    // Three switches, each two 25 px rows 256 wide from x 255: a click on the
    // second row selects it, one on the first selects it back.
    struct E10Switch {
        Script::Switch* widget;
        float x;
        float y;
        const char* row0;
        const char* row1;
    };
    const E10Switch e10Switches[] = {
        {&Script::g_keyboardP2, 255.0f, 424.0f, "Teclado para o jogador 2", "Jogador 2 s\xF3 no joystick"},
        {&Script::g_widescreen, 255.0f, 494.0f, "Tela larga (widescreen)", "Tela 4:3 (original)"},
        {&Script::g_language, 255.0f, 564.0f, "Portugu\xEAs", "English"},
        // E13's, in the second column (x 540-796) beside E8's.
        {&Script::g_pauseOnFocusLoss, 540.0f, 694.0f, "Pausa ao perder o foco", "Continua sem o foco"},
    };
    for (const E10Switch& row : e10Switches) {
        CHECK_EQ(row.widget->getCurrent(), 0u);
        click(vector2(row.x + 45.0f, row.y + 37.0f));
        std::printf("  '%s': switch %u\n", Utf8(row.row1).c_str(), row.widget->getCurrent());
        CHECK_EQ(row.widget->getCurrent(), 1u);
        CHECK(WaitForHud(g, string("[\x95] ") + row.row1, 3));
        CHECK(HudHas(g.m, string("[ ] ") + row.row0));
        click(vector2(row.x + 45.0f, row.y + 12.0f));
        CHECK_EQ(row.widget->getCurrent(), 0u);
        CHECK(WaitForHud(g, string("[\x95] ") + row.row0, 3));
    }
    // None of them moved the original's switches, nor E8's beside E13's.
    CHECK_EQ(Script::g_smoothMotion.getCurrent(), 0u);
    CHECK_EQ(Script::g_enablePS.getCurrent(), 0u);
    CHECK_EQ(Script::g_windowed.getCurrent(), 0u);
    CHECK_EQ(Script::g_controls.getCurrent(), 0u);

    // Two steppers, one 25 px row each from x 255: the label column is 180 px,
    // then "[<]" in x 435-475, the value, "[>]" in x 535-575. They stop at 0
    // and at 100% rather than wrapping.
    const auto stepper = [&](Script::Stepper& widget, const float y, const char* name) {
        const vector2 less(455.0f, y + 12.0f);
        const vector2 more(555.0f, y + 12.0f);
        CHECK_EQ(widget.getSteps(), 10u);
        CHECK_EQ(widget.getCurrent(), 10u);
        click(less);
        CHECK_EQ(widget.getCurrent(), 9u);
        CHECK(WaitForHud(g, "90%", 3));
        click(less);
        CHECK_EQ(widget.getCurrent(), 8u);
        click(more);
        click(more);
        CHECK_EQ(widget.getCurrent(), 10u);
        click(more);
        std::printf("  %s: [>] at 100%% leaves %u\n", name, widget.getCurrent());
        CHECK_EQ(widget.getCurrent(), 10u);
        for (int i = 0; i < 10; ++i) click(less);
        CHECK_EQ(widget.getCurrent(), 0u);
        CHECK(widget.getFraction() == 0.0f);
        click(less);
        std::printf("  %s: [<] at 0%% leaves %u\n", name, widget.getCurrent());
        CHECK_EQ(widget.getCurrent(), 0u);
        for (int i = 0; i < 10; ++i) click(more);
        CHECK_EQ(widget.getCurrent(), 10u);
        CHECK(widget.getFraction() == 1.0f);
    };
    stepper(Script::g_musicVolume, 634.0f, "music");
    CHECK_EQ(Script::g_effectsVolume.getCurrent(), 10u);   // the music's clicks left it alone
    stepper(Script::g_effectsVolume, 659.0f, "effects");
    CHECK_EQ(Script::g_musicVolume.getCurrent(), 10u);

    // What the layer seeds and compares with (the settings hold 0..1 floats):
    // the nearest step, so a hand-edited 0.75 reads as 8 and is not rewritten.
    CHECK_EQ(Script::g_musicVolume.stepFor(0.75f), 8u);
    CHECK_EQ(Script::g_musicVolume.stepFor(0.7f), 7u);
    CHECK_EQ(Script::g_musicVolume.stepFor(1.0f), 10u);
    CHECK_EQ(Script::g_musicVolume.stepFor(0.0f), 0u);
    CHECK_EQ(Script::g_musicVolume.stepFor(-1.0f), 0u);
    CHECK_EQ(Script::g_musicVolume.stepFor(2.0f), 10u);
    CHECK_EQ(Script::g_musicVolume.stepFor(std::nanf("")), 0u);
    Script::g_musicVolume.setCurrent(42u);
    CHECK_EQ(Script::g_musicVolume.getCurrent(), 10u);

    g.Steps(10);
    g.Step(g.With({K_ESC}));
    const int backAfter = WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/menu.esc"; });
    std::printf("  ESC: menu.esc loaded %d frames later\n", backAfter);
    CHECK(backAfter >= 0);
    g.Steps(10);
}

// === 15. level2 by the K_2 cheat, and the play_sound markers =================================
//
// setupScene.as:174-175 collects the markers named exactly "play_sound" into
// g_sounds; doLoop (setupScene.as:315-326) plays each one's sample on the first
// frame it is within the screen (+/-SIZE_TOLERANCE) and deletes it, so each
// plays once. Only level2 has such markers: 460 (-391,-74) and 463
// (-1149,-77), both horror.mp3.
//
// ORIGINAL BUG, pinned rather than fixed (it only loses a sound): five more
// horror.mp3 markers carry the FILE name "play_sound.ent" as their entity name
// - level2's 469, 493 and 548, level3's 219 and 427 - and GetEntityArray
// compares names exactly (ETHScene.cpp:1620-1631), so setupScene.as:175 never
// collects them and they never play.
void ScenarioPlaySound(Game& g) {
    CHECK(EnsureMenu(g));
    std::printf("-- New game with 2 held (main.as:110-111): level2\n");
    g.base.cursor = kNewGameButton;
    g.Steps(3);
    CHECK(LastButton() == "novo_jogo");
    g.Step(g.With({K_RETURN, K_2}));
    const InputFrame hold2 = g.With({K_2});
    const int loaded = WaitFor(g, 200, [] { return GetSceneFileName() == "scenes/level2.esc"; }, &hold2);
    const uint loadFrame = g.Frame();
    std::printf("  level2.esc loaded %d frames after the press, loop '%s'\n", loaded, g.m.LoopFunction().c_str());
    CHECK(loaded >= 180 && loaded <= 183);
    CHECK(g.m.LoopFunction() == "levelLoop");
    CHECK_EQ(Script::g_lives, 13);
    CHECK_EQ(Script::g_exp[0], 0);
    CHECK_EQ(Script::g_charLevel[0], 1);
    const ETHEntity p = WaitForPlayerReady(g, 240, "level2 start");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard in level2");
        return;
    }
    Trace(g, "level2 running");
    CHECK(std::fabs(p->GetPosition().x - 173.0f) < 2.0f);

    std::printf("-- what setupScene collected\n");
    std::set<int> collected;
    std::printf("  g_sounds:");
    for (const ETHEntity& s : Script::g_sounds) {
        std::printf(" %d '%s' %s at (%.0f,%.0f) alive %d;", s->GetID(), s->GetEntityName().c_str(),
                    s->GetStringData("name").c_str(), s->GetPosition().x, s->GetPosition().y, s->IsAlive() ? 1 : 0);
        collected.insert(s->GetID());
        CHECK(s->GetStringData("name") == "horror.mp3");
    }
    std::printf("\n");
    // E15 (Script.hpp kPlaySoundEntFix) collects the misnamed markers too.
    CHECK(collected == (Script::kPlaySoundEntFix ? std::set<int>{460, 463, 469, 493, 548} : std::set<int>{460, 463}));
    ETHEntityArray misnamed;
    GetEntityArray("play_sound.ent", misnamed);
    std::set<int> misnamedIds;
    std::printf("  named 'play_sound.ent' (never collected, ORIGINAL BUG setupScene.as:175):");
    for (const ETHEntity& s : misnamed) {
        std::printf(" %d %s at (%.0f,%.0f);", s->GetID(), s->GetStringData("name").c_str(), s->GetPosition().x,
                    s->GetPosition().y);
        misnamedIds.insert(s->GetID());
    }
    std::printf("\n");
    CHECK(misnamedIds == (std::set<int>{469, 493, 548}));

    const ETHEntity m460 = SeekEntity(460);
    CHECK(m460 != nullptr);
    if (m460 == nullptr) return;
    const vector2 cam = GetCameraPos();
    std::printf("  camera (%.0f,%.0f), screen %.0fx%.0f: marker 460 at (%.0f,%.0f) alive %s; horror.mp3 since the "
                "load: %s\n",
                cam.x, cam.y, GetScreenSize().x, GetScreenSize().y, m460->GetPosition().x, m460->GetPosition().y,
                m460->IsAlive() ? "yes" : "NO", g.sound.PlayedSince("horror.mp3", loadFrame) ? "PLAYED" : "not played");
    CHECK(m460->IsAlive());
    CHECK(!g.sound.PlayedSince("horror.mp3", loadFrame));

    std::printf("-- marker 460 on screen: horror.mp3 once, the marker deleted\n");
    const vector2 wiz = p->GetPositionXY();
    std::printf("  SHORTCUT: play_sound marker 460 moved to (%.0f,%.0f)\n", wiz.x + 120.0f, wiz.y - 40.0f);
    const uint moved = g.Frame() + 1;
    m460->SetPosition(vector3(wiz.x + 120.0f, wiz.y - 40.0f, 0.0f));
    const int played = WaitFor(g, 3, [&] { return g.sound.PlayedSince("horror.mp3", moved); });
    std::printf("  horror.mp3 played %d frame(s) after the move; the marker alive %s\n", played,
                m460->IsAlive() ? "YES" : "no");
    CHECK_EQ(played, 1);
    CHECK(!m460->IsAlive());
    g.Steps(120);
    std::printf("  120 frames later horror.mp3 has played %u time(s)\n", g.sound.PlayCount("horror.mp3", moved));
    CHECK_EQ(g.sound.PlayCount("horror.mp3", moved), 1u);

    std::printf("-- a 'play_sound.ent' marker on screen: silent in the original, plays under E15\n");
    const ETHEntity m493 = SeekEntity(493);
    CHECK(m493 != nullptr);
    if (m493 == nullptr) return;
    std::printf("  SHORTCUT: 'play_sound.ent' marker 493 moved to (%.0f,%.0f)\n", wiz.x + 120.0f, wiz.y - 40.0f);
    const uint moved2 = g.Frame() + 1;
    m493->SetPosition(vector3(wiz.x + 120.0f, wiz.y - 40.0f, 0.0f));
    g.Steps(60);
    std::printf("  60 frames on screen: horror.mp3 %s, marker 493 alive %s\n",
                g.sound.PlayedSince("horror.mp3", moved2) ? "PLAYED" : "not played", m493->IsAlive() ? "yes" : "NO");
    if (Script::kPlaySoundEntFix) {
        // E15: it plays once on screen and goes, like the correctly named ones.
        CHECK_EQ(g.sound.PlayCount("horror.mp3", moved2), 1u);
        CHECK(!m493->IsAlive());
    } else {
        CHECK(!g.sound.PlayedSince("horror.mp3", moved2));
        CHECK(m493->IsAlive());
    }
}

// === 16. level2 -> level3 through level2's own exit ===============================================

void ScenarioLevel2ToLevel3(Game& g) {
    const ETHEntity p = EnsureLevel(g, "scenes/level2.esc");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard");
        return;
    }
    // level2's next_level id 428 at (320,-1864), name=level3.esc.
    const ETHEntity door = SeekEntity(428);
    CHECK(door != nullptr);
    if (door == nullptr) return;
    CHECK(door->GetEntityName() == "next_level");
    const string target = door->GetStringData("name");
    std::printf("  next_level 428's 'name' = '%s'\n", target.c_str());
    CHECK(target == "level3.esc");
    const int lives = Script::g_lives;
    const int exp0 = Script::g_exp[0];
    const int exp1 = Script::g_exp[1];
    const int level0 = Script::g_charLevel[0];
    const int level1 = Script::g_charLevel[1];
    const uint elapsed = Script::g_timer.getElapsedTime();

    // SHORTCUT: minions 510 and 511 wait 80-120 px from the exit and would come
    // for him during the 3 s fade.
    std::printf("  SHORTCUT: wizard hp and maxHp set to 1000, teleported onto next_level 428; DOWN held\n");
    p->AddIntData("maxHp", 1000);
    p->AddIntData("hp", 1000);
    Teleport(p, door->GetPositionXY());
    const InputFrame down = g.With({K_DOWN});
    const int fading = WaitFor(g, 10, [&] { return door->CheckCustomData("fadeOut") != DT_NODATA; }, &down);
    std::printf("  fade started %d frames after the teleport, the wizard %.1f px from the exit (< 80)\n", fading,
                Dist(p, door));
    CHECK(fading >= 0);
    bool loading = false;
    int hpLow = p->GetIntData("hp");
    const int loaded = WaitFor(g, 200, [&] {
        loading = loading || HudHas(g.m, "Carregando...");
        if (GetSceneFileName() == "scenes/level2.esc" && p->IsAlive()) hpLow = std::min(hpLow, p->GetIntData("hp"));
        return GetSceneFileName() == "scenes/" + target;
    }, &down);
    std::printf("  %s loaded %d frames after the fade began (loop '%s'); 'Carregando...' %s; the wizard's hp during "
                "the fade went down to %d\n",
                ("scenes/" + target).c_str(), loaded, g.m.LoopFunction().c_str(), loading ? "shown" : "NOT shown",
                hpLow);
    CHECK(loaded >= 180 && loaded <= 182);
    CHECK(loading);
    CHECK(g.m.LoopFunction() == "levelLoop");
    const ETHEntity w = WaitForPlayerReady(g, 240, "level3 start");
    CHECK(Ready(w));
    if (!Ready(w)) return;
    Trace(g, "level3 running");
    // level3's wizard marker 1 at (218,227): a fresh bruxo.ent.
    std::printf("  level3: wizard at (%.1f,%.1f) (marker 1 at (218,227)), hp %d/%d; lives %d (were %d), exp %d/%d "
                "level %d/%d (were %d/%d, %d/%d); run timer %u ms (was %u)\n",
                w->GetPosition().x, w->GetPosition().y, w->GetIntData("hp"), w->GetIntData("maxHp"), Script::g_lives,
                lives, Script::g_exp[0], Script::g_exp[1], Script::g_charLevel[0], Script::g_charLevel[1], exp0, exp1,
                level0, level1, Script::g_timer.getElapsedTime(), elapsed);
    CHECK(std::fabs(w->GetPosition().x - 218.0f) < 2.0f);
    CHECK_EQ(w->GetIntData("hp"), 100);
    CHECK_EQ(w->GetIntData("maxHp"), 100);
    CHECK(w->CheckCustomData("hasCheckpoint") == DT_NODATA);
    // A level exit is not a new game: nothing of resetData runs.
    CHECK_EQ(Script::g_lives, lives);
    CHECK_EQ(Script::g_exp[0], exp0);
    CHECK_EQ(Script::g_exp[1], exp1);
    CHECK_EQ(Script::g_charLevel[0], level0);
    CHECK_EQ(Script::g_charLevel[1], level1);
    CHECK(Script::g_timer.getElapsedTime() >= elapsed + 3000u);
    // level3's two horror markers are both named "play_sound.ent" (scenario 15).
    std::printf("  level3 g_sounds %u; 'play_sound.ent' markers:", Script::g_sounds.size());
    ETHEntityArray misnamed;
    GetEntityArray("play_sound.ent", misnamed);
    std::set<int> ids;
    for (const ETHEntity& s : misnamed) {
        std::printf(" %d", s->GetID());
        ids.insert(s->GetID());
    }
    std::printf("\n");
    // E15 (kPlaySoundEntFix) collects them; the original collected none.
    CHECK_EQ(Script::g_sounds.size(), Script::kPlaySoundEntFix ? 2u : 0u);
    CHECK(ids == (std::set<int>{219, 427}));
    // level3's 'play' 1106 at (444,190) restarts the level music and goes.
    std::printf("  fase.mp3 looping %s; play 1106 %s\n", g.sound.Looping("fase.mp3") ? "yes" : "NO",
                SeekEntity(1106) == nullptr ? "gone" : "STILL THERE");
    CHECK(g.sound.Looping("fase.mp3"));
    CHECK(SeekEntity(1106) == nullptr);
}

// === 17. The paladin and the master knight ==========================================================
//
// meleeCharacterAI (characterAI.as:117-199) with their data.enml rows. The
// paladin: 400 hp, paladin_sword 20, fire resistant, and jumpBackAfterAttack -
// after each swing it knocks itself (+-12,-12) away from the wizard
// (characterAI.as:189-194). The master knight: 1700 hp, dark_sword 25,
// waitBeforeAttack - it swings only once the wizard has been in its reach for
// its whole coolDown, counted from the last frame he was not (swords.as:51-62)
// - pushBackBias 0.05, and it dies in fade_out_beam_large
// (controlCharacters.as:582). Both spawn from their markers with their
// showUpSfx (setupScene.as:302-306).

// Swings the wizard's sword (S), facing right, whenever `enemy` is 0-`reach` px
// to his right and 15 frames have passed since the last swing, until the
// enemy's hp drops (untilDead false) or reaches 0, or `limit` frames pass.
// Returns each hp drop.
std::vector<int> SwordAt(Game& g, const ETHEntity& p, const ETHEntity& enemy, const float reach, const uint limit,
                         const bool untilDead, Spotter* spot) {
    std::vector<int> drops;
    int hp = enemy->GetIntData("hp");
    uint lastPress = 0;
    for (uint i = 0; i < limit && enemy->IsAlive() && enemy->GetIntData("hp") > 0; ++i) {
        const float dx = enemy->GetPosition().x - p->GetPosition().x;
        if (dx > 0.0f && dx < reach && g.Frame() >= lastPress + 15) {
            lastPress = g.Frame();
            g.Step(g.With({K_S}));
        } else {
            g.Step();
        }
        if (spot != nullptr) spot->Poll();
        const int now = enemy->GetIntData("hp");
        if (now < hp) drops.push_back(hp - now);
        hp = now;
        if (!untilDead && !drops.empty()) break;
    }
    return drops;
}

// The `file` entity doLoop spawns from `marker` once it is moved to `at`, or
// null.
ETHEntity SpawnFromMarker(Game& g, const ETHEntity& marker, const vector2& at, const string& file) {
    const int since = GetLastID();
    marker->SetPosition(vector3(at.x, at.y, 0.0f));
    ETHEntity spawned;
    WaitFor(g, 3, [&] {
        const ETHEntityArray found = NewEntities(file, since);
        if (!found.empty()) spawned = found[0];
        return spawned != nullptr;
    });
    return spawned;
}

void ScenarioPaladinAndMasterKnight(Game& g) {
    const ETHEntity p = EnsureLevel(g, "scenes/level3.esc");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard");
        return;
    }
    // SHORTCUT: the wizard's hp and maxHp set to 1000 so that he lives through
    // both fights (addToHp clamps at maxHp).
    std::printf("  SHORTCUT: wizard hp and maxHp set to 1000 for both fights\n");
    p->AddIntData("maxHp", 1000);
    p->AddIntData("hp", 1000);
    if (p->GetUIntData("currentDir") != Script::RIGHT) {
        g.Step(g.With({K_RIGHT}));
        g.Steps(10);
    }
    WaitForPlayerReady(g, 60, "paladin");

    std::printf("-- the paladin\n");
    const ETHEntity pm = FindMarker("paladin", 209);
    CHECK(pm != nullptr);
    if (pm == nullptr) return;
    const vector2 wiz = p->GetPositionXY();
    std::printf("  SHORTCUT: paladin marker %d moved to (%.0f,%.0f)\n", pm->GetID(), wiz.x + 90.0f, wiz.y - 20.0f);
    const int since = GetLastID();
    const ETHEntity paladin = SpawnFromMarker(g, pm, wiz + vector2(90.0f, -20.0f), "paladin.ent");
    CHECK(paladin != nullptr);
    if (paladin == nullptr) return;
    const uint spawnFrame = g.Frame();
    std::printf("  paladin id %d: hp %d damage %d speed %.0f viewRadius %.0f attackRadius %.0f coolDown %u "
                "waitBeforeAttack %u jumpBackAfterAttack %u fireResistant %u pushBackBias %.2f expGiven %d; "
                "paladin_appear.mp3 on its spawn frame %s\n",
                paladin->GetID(), paladin->GetIntData("hp"), paladin->GetIntData("damage"),
                paladin->GetFloatData("speed"), paladin->GetFloatData("viewRadius"),
                paladin->GetFloatData("attackRadius"), paladin->GetUIntData("coolDown"),
                paladin->GetUIntData("waitBeforeAttack"), paladin->GetUIntData("jumpBackAfterAttack"),
                paladin->GetUIntData("fireResistant"), paladin->GetFloatData("pushBackBias"),
                paladin->GetIntData("expGiven"), g.sound.PlayedAt("paladin_appear.mp3", spawnFrame) ? "yes" : "NO");
    CHECK(!pm->IsAlive());
    CHECK_EQ(paladin->GetIntData("hp"), 400);
    CHECK_EQ(paladin->GetIntData("damage"), 20);
    CHECK_NEAR(paladin->GetFloatData("speed"), 80.0f);
    CHECK_NEAR(paladin->GetFloatData("attackRadius"), 47.0f);
    CHECK_EQ(paladin->GetUIntData("coolDown"), 550u);
    CHECK_EQ(paladin->GetUIntData("waitBeforeAttack"), 0u);
    CHECK_EQ(paladin->GetUIntData("jumpBackAfterAttack"), 1u);
    CHECK_EQ(paladin->GetUIntData("fireResistant"), 1u);
    CHECK_NEAR(paladin->GetFloatData("pushBackBias"), 0.05f);
    CHECK_EQ(paladin->GetIntData("expGiven"), 400);
    CHECK(g.sound.PlayedAt("paladin_appear.mp3", spawnFrame));

    std::printf("-- it chases, swings (paladin_sword, 20) and jumps back\n");
    Spotter ps(since, {"paladin_sword.ent", "fade_out_beam.ent", "fade_out_beam_large.ent", "hit_fail.ent"});
    const float px0 = paladin->GetPosition().x;
    bool chasing = false;
    int hpW = p->GetIntData("hp");
    int firstDrop = 0;
    float kbx = 0.0f;
    float kby = 0.0f;
    const int attacked = WaitFor(g, 300, [&] {
        ps.Poll();
        if (paladin->GetUIntData("action") == Script::CHASING) chasing = true;
        for (const auto& entry : ps.seen["paladin_sword.ent"]) {
            if (entry.second->GetIntData("ownerID") == paladin->GetID() && kbx == 0.0f) {
                kbx = paladin->GetFloatData("knockBackX");
                kby = paladin->GetFloatData("knockBackY");
            }
        }
        const int hp = p->GetIntData("hp");
        if (hp < hpW && firstDrop == 0) firstDrop = hpW - hp;
        hpW = hp;
        return firstDrop != 0 && kbx != 0.0f;
    });
    // util.as:188-218 move(): the knock is capped at 0.9 * 0.4 * the smaller
    // side of the entity, then scaled by 0.8.
    const vector2 palSize = paladin->GetSize();
    const float cap = std::min(palSize.x, palSize.y) * 0.4f;
    const float jumpLength = std::sqrt(12.0f * 12.0f * 2.0f);
    const float jumpX = (jumpLength >= cap ? 12.0f / jumpLength * cap * 0.9f : 12.0f) * 0.8f;
    std::printf("  swung after %d frames (x %.1f -> %.1f, chasing seen %s): wizard -%d; the paladin's knockBack then "
                "(%.2f,%.2f) (expected (%.2f,%.2f): (12,-12) away from him through one move, size %.0fx%.0f)\n",
                attacked, px0, paladin->GetPosition().x, chasing ? "yes" : "NO", firstDrop, kbx, kby, jumpX, -jumpX,
                palSize.x, palSize.y);
    CHECK(attacked >= 0);
    CHECK(chasing);
    CHECK_EQ(firstDrop, 20);
    CHECK(kbx > 0.0f);
    CHECK(kby < 0.0f);
    CHECK(std::fabs(kbx - jumpX) < 0.05f);
    CHECK(std::fabs(kby + jumpX) < 0.05f);

    std::printf("-- the wizard's fireball (D): fire resistant, damage / 5 and hit_fail\n");
    // SHORTCUT: mana restored; the paladin put 150 px away, out of its reach,
    // once its sword is gone (a fireball also bursts on a blow it meets).
    p->AddIntData("mp", p->GetIntData("maxMp"));
    WaitFor(g, 60, [] {
        ETHEntityArray swords;
        GetEntityArray("paladin_sword.ent", swords);
        return swords.empty();
    });
    WaitForPlayerReady(g, 60, "before the fireball");
    Teleport(paladin, p->GetPositionXY() + vector2(150.0f, -10.0f));
    std::printf("  SHORTCUT: mp set to maxMp, the paladin teleported 150 px to the wizard's right\n");
    g.Steps(8);
    CHECK_EQ(p->GetUIntData("currentDir"), Script::RIGHT);
    const int palHp0 = paladin->GetIntData("hp");
    Spotter fs(GetLastID(), {"fire_ball.ent", "hit_fail.ent", "explosion.ent"});
    g.Step(g.With({K_D}));
    fs.Poll();
    const ETHEntity ball = fs.First("fire_ball.ent");
    CHECK(ball != nullptr);
    if (ball != nullptr) CHECK_EQ(ball->GetIntData("damage"), 30);
    const int hit = WaitFor(g, 90, [&] {
        fs.Poll();
        return paladin->GetIntData("hp") != palHp0;
    });
    fs.Poll();
    const int fireExpected = MainCharDamage(30, Script::g_charLevel[0]) / 5;
    std::printf("  paladin hp %d -> %d after %d frames (expected -%d = (30 + int(30*%d/4)) / 5); hit_fail.ent %u, "
                "explosion.ent %u\n",
                palHp0, paladin->GetIntData("hp"), hit, fireExpected, Script::g_charLevel[0], fs.Count("hit_fail.ent"),
                fs.Count("explosion.ent"));
    CHECK(hit > 0);
    CHECK_EQ(palHp0 - paladin->GetIntData("hp"), fireExpected);
    CHECK(fs.Count("hit_fail.ent") >= 1u);
    CHECK_EQ(fs.Count("explosion.ent"), 0u);

    std::printf("-- its last hit points go to the sword: 400 experience to both players\n");
    int e0 = Script::g_exp[0], l0 = Script::g_charLevel[0];
    int e1 = Script::g_exp[1], l1 = Script::g_charLevel[1];
    const int exp0 = e0, exp1 = e1;
    // SHORTCUT: 400 hp is sixteen blows.
    std::printf("  SHORTCUT: paladin hp set to 20 (a sword blow is %d)\n", MainCharDamage(20, l0));
    paladin->AddIntData("hp", 20);
    const std::vector<int> palDrops = SwordAt(g, p, paladin, 60.0f, 600, true, &ps);
    std::printf("  paladin hp drops:");
    for (const int d : palDrops) std::printf(" -%d", d);
    std::printf("; hp %d\n", paladin->GetIntData("hp"));
    CHECK(paladin->GetIntData("hp") <= 0);
    const int deathAt = WaitFor(g, 3, [&] {
        ps.Poll();
        return paladin->CheckCustomData("deathTime") != DT_NODATA;
    });
    CHECK(deathAt >= 0);
    const uint palDeath = g.Frame();
    ExpectedExp(e0, l0, 400);
    ExpectedExp(e1, l1, 400);
    std::printf("  exp player0 %d -> %d (level %d), player1 %d -> %d (level %d); expected %d/%d and %d/%d; "
                "fade_out_beam.ent %u\n",
                exp0, Script::g_exp[0], Script::g_charLevel[0], exp1, Script::g_exp[1], Script::g_charLevel[1], e0, l0,
                e1, l1, ps.Count("fade_out_beam.ent"));
    CHECK_EQ(Script::g_exp[0], e0);
    CHECK_EQ(Script::g_charLevel[0], l0);
    CHECK_EQ(Script::g_exp[1], e1);
    CHECK_EQ(Script::g_charLevel[1], l1);
    CHECK(ps.Count("fade_out_beam.ent") >= 1u);
    const int palGone = WaitFor(g, 200, [&] { return !paladin->IsAlive(); });
    std::printf("  deleted %u frames after its death frame\n", g.Frame() - palDeath);
    CHECK(palGone >= 0);
    CHECK_EQ(g.Frame() - palDeath, 180u);

    std::printf("-- the master knight\n");
    WaitForPlayerReady(g, 120, "master knight");
    if (p->GetUIntData("currentDir") != Script::RIGHT) {
        g.Step(g.With({K_RIGHT}));
        g.Steps(10);
    }
    const ETHEntity mm = FindMarker("master_knight", 645);
    CHECK(mm != nullptr);
    if (mm == nullptr) return;
    const vector2 wiz2 = p->GetPositionXY();
    std::printf("  SHORTCUT: master_knight marker %d moved to (%.0f,%.0f)\n", mm->GetID(), wiz2.x + 150.0f,
                wiz2.y - 60.0f);
    const int sinceM = GetLastID();
    const ETHEntity mk = SpawnFromMarker(g, mm, wiz2 + vector2(150.0f, -60.0f), "master_knight.ent");
    CHECK(mk != nullptr);
    if (mk == nullptr) return;
    const uint mkSpawn = g.Frame();
    std::printf("  master knight id %d: hp %d damage %d speed %.0f viewRadius %.0f attackRadius %.0f coolDown %u "
                "waitBeforeAttack %u jumpBackAfterAttack %u fireResistant %u pushBackBias %.2f expGiven %d; "
                "creature_show_up.mp3 on its spawn frame %s\n",
                mk->GetID(), mk->GetIntData("hp"), mk->GetIntData("damage"), mk->GetFloatData("speed"),
                mk->GetFloatData("viewRadius"), mk->GetFloatData("attackRadius"), mk->GetUIntData("coolDown"),
                mk->GetUIntData("waitBeforeAttack"), mk->GetUIntData("jumpBackAfterAttack"),
                mk->GetUIntData("fireResistant"), mk->GetFloatData("pushBackBias"), mk->GetIntData("expGiven"),
                g.sound.PlayedAt("creature_show_up.mp3", mkSpawn) ? "yes" : "NO");
    CHECK_EQ(mk->GetIntData("hp"), 1700);
    CHECK_EQ(mk->GetIntData("damage"), 25);
    CHECK_NEAR(mk->GetFloatData("attackRadius"), 80.0f);
    CHECK_EQ(mk->GetUIntData("coolDown"), 420u);
    // doLoop wrote 0 (setupScene.as:282); spawn() then read data.enml's 1.
    CHECK_EQ(mk->GetUIntData("waitBeforeAttack"), 1u);
    CHECK_EQ(mk->GetUIntData("jumpBackAfterAttack"), 0u);
    CHECK_EQ(mk->GetUIntData("fireResistant"), 0u);
    CHECK_NEAR(mk->GetFloatData("pushBackBias"), 0.05f);
    CHECK_EQ(mk->GetIntData("expGiven"), 1700);
    CHECK(g.sound.PlayedAt("creature_show_up.mp3", mkSpawn));

    std::printf("-- it chases, then waits its whole coolDown in reach before it swings (dark_sword, 25)\n");
    Spotter ms(sinceM, {"dark_sword.ent", "fade_out_beam_large.ent"});
    const float mx0 = mk->GetPosition().x;
    uint prevLtds = mk->GetUIntData("lastTimeDidntSee");
    uint prevTime = GetTime();
    uint ltdsBefore = 0;
    uint timeBefore = 0;
    uint swordTime = 0;
    uint firstReachTime = 0;
    bool mkChasing = false;
    ETHEntity darkSword;
    int hpM = p->GetIntData("hp");
    int mkDrop = 0;
    const int mkSwung = WaitFor(g, 400, [&] {
        ms.Poll();
        if (mk->GetUIntData("action") == Script::CHASING) mkChasing = true;
        if (darkSword == nullptr) {
            for (const auto& entry : ms.seen["dark_sword.ent"]) {
                if (entry.second->GetIntData("ownerID") == mk->GetID()) {
                    darkSword = entry.second;
                    swordTime = GetTime();
                    ltdsBefore = prevLtds;
                    timeBefore = prevTime;
                }
            }
        }
        if (firstReachTime == 0 && Dist(mk, p) <= 80.0f) firstReachTime = GetTime();
        prevLtds = mk->GetUIntData("lastTimeDidntSee");
        prevTime = GetTime();
        const int hp = p->GetIntData("hp");
        if (hp < hpM && mkDrop == 0) mkDrop = hpM - hp;
        hpM = hp;
        return darkSword != nullptr && mkDrop != 0;
    });
    std::printf("  x %.1f -> %.1f, chasing seen %s; first within 80 px at %u ms; its last frame without him in reach "
                "(lastTimeDidntSee) %u ms; the frame before the swing %u ms (+%u); dark_sword at %u ms (+%u); wizard "
                "-%d\n",
                mx0, mk->GetPosition().x, mkChasing ? "yes" : "NO", firstReachTime, ltdsBefore, timeBefore,
                timeBefore - ltdsBefore, swordTime, swordTime - ltdsBefore, mkDrop);
    CHECK(mkSwung >= 0);
    CHECK(mkChasing);
    CHECK(mk->GetPosition().x < mx0 - 20.0f);
    // swords.as:57: (GetTime()-lastTimeDidntSee) < coolDown holds it back; on
    // the first frame it does not, it swings.
    CHECK(swordTime - ltdsBefore >= 420u);
    CHECK(timeBefore - ltdsBefore < 420u);
    CHECK_EQ(mkDrop, 25);

    std::printf("-- the wizard's sword on it: pushed by only 4 * pushBackBias 0.05\n");
    const int mkHp0 = mk->GetIntData("hp");
    const std::vector<int> firstHit = SwordAt(g, p, mk, 80.0f, 300, false, &ms);
    const float mkKbx = mk->GetFloatData("knockBackX");
    const int swordExpected = MainCharDamage(20, Script::g_charLevel[0]);
    std::printf("  master knight hp %d -> %d (expected -%d = 20 + int(20*%d/4)); its knockBackX then %.3f (a "
                "warrior's would be up to 4)\n",
                mkHp0, mk->GetIntData("hp"), swordExpected, Script::g_charLevel[0], mkKbx);
    CHECK_EQ(static_cast<uint>(firstHit.size()), 1u);
    if (!firstHit.empty()) CHECK_EQ(firstHit[0], swordExpected);
    CHECK(mkKbx > 0.0f && mkKbx <= 0.2001f);

    std::printf("-- it falls to the sword: fade_out_beam_large, 1700 experience to both players\n");
    int f0 = Script::g_exp[0], m0 = Script::g_charLevel[0];
    int f1 = Script::g_exp[1], m1 = Script::g_charLevel[1];
    const int mexp0 = f0, mexp1 = f1;
    // SHORTCUT: 1700 hp is some sixty blows.
    std::printf("  SHORTCUT: master knight hp set to 20\n");
    mk->AddIntData("hp", 20);
    SwordAt(g, p, mk, 80.0f, 600, true, &ms);
    CHECK(mk->GetIntData("hp") <= 0);
    const int mkDeathAt = WaitFor(g, 3, [&] {
        ms.Poll();
        return mk->CheckCustomData("deathTime") != DT_NODATA;
    });
    CHECK(mkDeathAt >= 0);
    const uint mkDeath = g.Frame();
    ExpectedExp(f0, m0, 1700);
    ExpectedExp(f1, m1, 1700);
    std::printf("  exp player0 %d -> %d (level %d), player1 %d -> %d (level %d); expected %d/%d and %d/%d; "
                "fade_out_beam_large.ent %u\n",
                mexp0, Script::g_exp[0], Script::g_charLevel[0], mexp1, Script::g_exp[1], Script::g_charLevel[1], f0,
                m0, f1, m1, ms.Count("fade_out_beam_large.ent"));
    CHECK_EQ(Script::g_exp[0], f0);
    CHECK_EQ(Script::g_charLevel[0], m0);
    CHECK_EQ(Script::g_exp[1], f1);
    CHECK_EQ(Script::g_charLevel[1], m1);
    CHECK(ms.Count("fade_out_beam_large.ent") >= 1u);
    const int mkGone = WaitFor(g, 200, [&] { return !mk->IsAlive(); });
    std::printf("  deleted %u frames after its death frame\n", g.Frame() - mkDeath);
    CHECK(mkGone >= 0);
    CHECK_EQ(g.Frame() - mkDeath, 180u);
    p->AddIntData("maxHp", 100);
    p->AddIntData("hp", 100);
    std::printf("  SHORTCUT: wizard hp and maxHp back to 100\n");
}

// === 18. The summon's price and its refusals ==========================================================
//
// player1Summoner (controlCharacters.as:674-731): START on player 2's pad (his
// JK_10, playerInput.as:281-283) costs the wizard 50 mana - refused at 50 or
// less (`<= 50`, :681) - and a life, and puts the princess one box-width plus
// 1 px to his right and 6 px up: bruxo.ent's box is (0,4) 14x36, so at his
// position + (15,-2). Refused while a princess is VISIBLE (:688-697), or where
// a collidable box fills that spot (:700-718).
void ScenarioSummonRules(Game& g) {
    const ETHEntity p = LoadLevel(g, "scenes/level1.esc");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard");
        return;
    }
    CHECK_EQ(Script::g_sounds.size(), 0u);   // level1 has no play_sound marker
    std::printf("-- two pads (E12): player 1's on index 1, player 2's on index 0\n");
    g.base.pads[0].connected = true;
    g.base.pads[1].connected = true;
    g.Steps(2);
    CHECK(Script::hasASecondController());
    CHECK_EQ(Script::getPlayerJoystick(0), 1u);
    CHECK_EQ(Script::getPlayerJoystick(1), 0u);
    InputFrame start = g.base;
    start.pads[0].buttons[static_cast<std::size_t>(JK_10)] = true;
    // A START press: one frame down, one up (the next press is a fresh HIT).
    const auto pressStart = [&g, &start] {
        g.Step(start);
        g.Step();
    };
    // The mana a step asks for, with doMpRecovery's clock reset so that it
    // gives nothing back on the frame of the press (it runs before
    // player1Summoner in ETHCallback_bruxo, controlCharacters.as:286-289).
    const auto setMp = [&p](const int mp) {
        p->AddIntData("mp", mp);
        p->AddUIntData("lastMpIncr", GetTime());
    };
    const int lives = Script::g_lives;

    std::printf("-- 50 mana is not enough\n");
    std::printf("  SHORTCUT: mp set to 50 (and lastMpIncr to now)\n");
    setMp(50);
    int since = GetLastID();
    pressStart();
    std::printf("  princess.ent new %u, mp %d, lives %d\n", static_cast<uint>(NewEntities(kPrincess, since).size()),
                p->GetIntData("mp"), Script::g_lives);
    CHECK(NewEntities(kPrincess, since).empty());
    CHECK_EQ(p->GetIntData("mp"), 50);
    CHECK_EQ(Script::g_lives, lives);
    CHECK(WaitForHud(g, "necess\xE1rio de 50 mana para invocar a criatura", 3));

    std::printf("-- no room to his right\n");
    // level1's tile01 10 at (482,352): box x 450-514, y 320-384.
    const ETHEntity block = SeekEntity(10);
    CHECK(block != nullptr);
    if (block != nullptr) CHECK(block->GetEntityName() == "tile01.ent");
    std::printf("  SHORTCUT: mp set to 100, the wizard teleported to (440,352): his box ends 3 px left of tile01 10\n");
    setMp(100);
    since = GetLastID();
    Teleport(p, vector2(440.0f, 352.0f));
    pressStart();
    std::printf("  princess.ent new %u, mp %d, lives %d\n", static_cast<uint>(NewEntities(kPrincess, since).size()),
                p->GetIntData("mp"), Script::g_lives);
    CHECK(NewEntities(kPrincess, since).empty());
    CHECK_EQ(Script::g_lives, lives);
    CHECK_EQ(p->GetIntData("mp"), 100);
    CHECK(WaitForHud(g, "Imposs\xEDvel invocar criatura daqui", 3));
    WaitForPlayerReady(g, 120, "after the fall");

    std::printf("-- 51 mana: she comes\n");
    std::printf("  SHORTCUT: mp set to 51\n");
    setMp(51);
    since = GetLastID();
    g.Step(start);
    const vector3 at = p->GetPosition();
    const ETHEntityArray princesses = NewEntities(kPrincess, since);
    const ETHEntityArray summons = NewEntities("summon.ent", since);
    CHECK_EQ(princesses.size(), 1u);
    CHECK_EQ(summons.size(), 1u);
    if (princesses.empty() || summons.empty()) return;
    const ETHEntity q = princesses[0];
    const vector3 summonAt = summons[0]->GetPosition();
    std::printf("  wizard at (%.2f,%.2f,%.2f); summon.ent at (%.2f,%.2f,%.2f) = + (%.2f,%.2f); princess at "
                "(%.2f,%.2f); mp %d, lives %d -> %d\n",
                at.x, at.y, at.z, summonAt.x, summonAt.y, summonAt.z, summonAt.x - at.x, summonAt.y - at.y,
                q->GetPosition().x, q->GetPosition().y, p->GetIntData("mp"), lives, Script::g_lives);
    CHECK(std::fabs(summonAt.x - at.x - 15.0f) < 0.01f);
    CHECK(std::fabs(summonAt.y - at.y + 2.0f) < 0.01f);
    CHECK(std::fabs(q->GetPosition().x - at.x - 15.0f) < 1.0f);
    CHECK_EQ(p->GetIntData("mp"), 1);
    CHECK_EQ(Script::g_lives, lives - 1);
    g.Step();
    CHECK(WaitForHud(g, "Criatura m\xE1gica invocada", 3));
    g.Steps(20);

    std::printf("-- a second one while she is on screen: refused\n");
    std::printf("  SHORTCUT: mp set to 100\n");
    setMp(100);
    since = GetLastID();
    pressStart();
    CHECK(NewEntities(kPrincess, since).empty());
    CHECK_EQ(Script::g_lives, lives - 1);
    CHECK_EQ(p->GetIntData("mp"), 100);
    CHECK(WaitForHud(g, "invocar 2 criaturas ao mesmo tempo", 3));

    std::printf("-- off screen she no longer counts: a second princess, a second life\n");
    // The refusal looks only at GetVisibleEntities (controlCharacters.as:
    // 688-697); the one off screen vanishes 3 s later on its own.
    std::printf("  SHORTCUT: the princess teleported 2000 px to the right\n");
    const uint setup = Script::g_levelStartTime;
    Teleport(q, q->GetPositionXY() + vector2(2000.0f, 0.0f));
    const uint teleported = g.Frame();
    g.Steps(2);
    ETHEntityArray visible;
    GetVisibleEntities(visible);
    bool qVisible = false;
    for (const ETHEntity& e : visible) qVisible = qVisible || e == q;
    std::printf("  SHORTCUT: mp set to 100\n");
    setMp(100);
    since = GetLastID();
    pressStart();
    const ETHEntityArray second = NewEntities(kPrincess, since);
    std::printf("  the first princess visible %s; after START: princess.ent new %u, mp %d, lives %d\n",
                qVisible ? "YES" : "no", static_cast<uint>(second.size()), p->GetIntData("mp"), Script::g_lives);
    CHECK(!qVisible);
    CHECK_EQ(second.size(), 1u);
    CHECK_EQ(p->GetIntData("mp"), 50);
    CHECK_EQ(Script::g_lives, lives - 2);
    const int died = WaitFor(g, 240, [&] { return q->GetIntData("hp") <= 0; });
    const uint diedAfter = g.Frame() - teleported;
    const int gone = WaitFor(g, 200, [&] { return !q->IsAlive(); });
    std::printf("  the first: hp 0 %u frames after its teleport, deleted %d frames later; the second alive %s hp %d; "
                "lives %d; scene set up again %s\n",
                diedAfter, gone, (!second.empty() && second[0]->IsAlive()) ? "yes" : "NO",
                second.empty() ? 0 : second[0]->GetIntData("hp"), Script::g_lives,
                Script::g_levelStartTime != setup ? "YES" : "no");
    CHECK(died >= 0);
    CHECK(diedAfter >= 180u && diedAfter <= 192u);
    CHECK(gone >= 180 && gone <= 183);
    if (!second.empty()) {
        CHECK(second[0]->IsAlive());
        CHECK(second[0]->GetIntData("hp") > 0);
    }
    CHECK_EQ(Script::g_lives, lives - 2);
    CHECK(Script::g_levelStartTime == setup);
    CHECK(Ready(Player()));
    g.base.pads[0].connected = false;
    g.base.pads[1].connected = false;
    g.Steps(5);
}

// === 19. Versus in arenas 2 to 6 ======================================================================
//
// Each arena as the arena select's thumbnail starts it (menu.as:338-342, then
// newGame("pvp_lvN.esc"), main.as:118-121): both players from their markers
// with hp and maxHp x5 (setupScene.as:289-293), a point for the wizard's
// killing blow (doDamage.as:170-183) and the arena set up again 3 s later
// (controlCharacters.as:443-449). Arena 6 is played to the end: 3 points.

// One point for the wizard: the princess put 30 px to his LEFT (each arena's
// wizard marker has floor on that side) with 20 hp, and his sword. Returns
// whether the point came.
bool PvpPoint(Game& g, const int round) {
    const int ready = WaitFor(g, 240, [] { return Ready(Player()) && Ready(Princess()); });
    CHECK(ready >= 0);
    if (ready < 0) return false;
    const ETHEntity w = Player();
    const ETHEntity q = Princess();
    const int points0 = Script::g_pvpPoints[0];
    const int points1 = Script::g_pvpPoints[1];
    int scored = -1;
    for (int attempt = 1; attempt <= 3 && scored < 0 && q->IsAlive() && q->GetIntData("hp") > 0; ++attempt) {
        // SHORTCUT: the princess teleported beside the wizard, her hp set to 20
        // (a sword blow is 25 or more).
        Teleport(q, w->GetPositionXY() + vector2(-30.0f, -4.0f));
        g.Steps(10);
        q->AddIntData("hp", 20);
        if (w->GetUIntData("currentDir") != Script::LEFT) {
            g.Step(g.With({K_LEFT}));
            g.Steps(15);
        }
        uint lastPress = 0;
        for (int i = 0; i < 180; ++i) {
            if (Script::g_pvpPoints[0] > points0) {
                scored = i;
                break;
            }
            const float dx = w->GetPosition().x - q->GetPosition().x;
            if (dx > 0.0f && dx < 50.0f && g.Frame() >= lastPress + 15) {
                lastPress = g.Frame();
                g.Step(g.With({K_S}));
            } else {
                g.Step();
            }
        }
        std::printf("  round %d, attempt %d: SHORTCUT princess 30 px left of the wizard with hp 20; point after %d "
                    "frames: %d - %d\n",
                    round, attempt, scored, Script::g_pvpPoints[0], Script::g_pvpPoints[1]);
    }
    CHECK(scored >= 0);
    CHECK_EQ(Script::g_pvpPoints[0], points0 + 1);
    CHECK_EQ(Script::g_pvpPoints[1], points1);
    return scored >= 0;
}

void ScenarioArenas(Game& g) {
    g.base.pads[0].connected = true;   // player 2's
    g.base.pads[1].connected = true;   // player 1's (E12)
    const uint matchStart = g.Frame();
    for (int n = 2; n <= 6; ++n) {
        const string scene = "pvp_lv" + std::to_string(n) + ".esc";
        std::printf("-- %s by newGame(\"%s\")\n", scene.c_str(), scene.c_str());
        const uint setup = Script::g_levelStartTime;
        Script::newGame(scene);
        const int set = WaitFor(g, 3, [&] { return Script::g_levelStartTime != setup; });
        std::printf("  set up %d frames later: %s, loop '%s'; lives %d, points %d - %d\n", set,
                    GetSceneFileName().c_str(), g.m.LoopFunction().c_str(), Script::g_lives, Script::g_pvpPoints[0],
                    Script::g_pvpPoints[1]);
        CHECK(set >= 0);
        CHECK(GetSceneFileName() == "scenes/" + scene);
        CHECK(g.m.LoopFunction() == "pvpLoop");
        CHECK_EQ(Script::g_lives, 13);
        CHECK_EQ(Script::g_pvpPoints[0], 0);
        CHECK_EQ(Script::g_pvpPoints[1], 0);
        const int both = WaitFor(g, 240, [] { return Ready(Player()) && Ready(Princess()); });
        if (both < 0) {
            CHECK_MSG(false, "both players did not stand in " + scene);
            Trace(g, scene.c_str());
            continue;
        }
        const ETHEntity w = Player();
        const ETHEntity q = Princess();
        std::printf("  wizard (%.1f,%.1f) hp %d/%d pvpMode %u; princess (%.1f,%.1f) hp %d/%d pvpMode %u; chefao.mp3 "
                    "looping %s\n",
                    w->GetPosition().x, w->GetPosition().y, w->GetIntData("hp"), w->GetIntData("maxHp"),
                    w->GetUIntData("pvpMode"), q->GetPosition().x, q->GetPosition().y, q->GetIntData("hp"),
                    q->GetIntData("maxHp"), q->GetUIntData("pvpMode"), g.sound.Looping("chefao.mp3") ? "yes" : "NO");
        CHECK_EQ(w->GetIntData("hp"), 500);
        CHECK_EQ(w->GetIntData("maxHp"), 500);
        CHECK_EQ(q->GetIntData("hp"), 500);
        CHECK_EQ(q->GetIntData("maxHp"), 500);
        CHECK_EQ(w->GetUIntData("pvpMode"), 1u);
        CHECK_EQ(q->GetUIntData("pvpMode"), 1u);
        CHECK(g.sound.Looping("chefao.mp3"));
        std::printf("  enemies:");
        for (const char* name : {"warrior", "knight", "minion", "impy", "paladin", "master_knight"}) {
            ETHEntityArray found;
            GetEntityArray(string(name) + ".ent", found);
            uint alive = 0;
            for (const ETHEntity& e : found) {
                if (!e->IsAlive()) continue;
                ++alive;
                CHECK_EQ(e->GetUIntData("pvpMode"), 1u);
            }
            if (alive != 0) std::printf(" %s %u", name, alive);
        }
        uint markers = 0;
        for (const ETHEntity& marker : Script::g_spawn) markers += marker->IsAlive() ? 1u : 0u;
        std::printf("; markers not yet spawned %u\n", markers);

        const int rounds = n == 6 ? Script::MAX_PVP_POINTS : 1;
        for (int round = 1; round <= rounds; ++round) {
            if (!PvpPoint(g, round)) break;
            if (Script::g_pvpPoints[0] >= Script::MAX_PVP_POINTS) break;
            const int reload = WaitForSetup(g, 200);
            const int back = WaitFor(g, 240, [] { return Ready(Player()) && Ready(Princess()); });
            std::printf("    %s set up again %d frames later; points still %d - %d; both standing again after %d, "
                        "hp %d and %d\n",
                        GetSceneFileName().c_str(), reload, Script::g_pvpPoints[0], Script::g_pvpPoints[1], back,
                        Player() != nullptr ? Player()->GetIntData("hp") : -1,
                        Princess() != nullptr ? Princess()->GetIntData("hp") : -1);
            CHECK(reload >= 180 && reload <= 184);
            CHECK(GetSceneFileName() == "scenes/" + scene);
            CHECK(back >= 0);
            if (back >= 0) {
                CHECK_EQ(Player()->GetIntData("hp"), 500);
                CHECK_EQ(Princess()->GetIntData("hp"), 500);
            }
        }
    }

    std::printf("-- pvp_lv6 to 3 points\n");
    const int won = WaitFor(g, 3, [] { return Script::g_gameFinished; });
    std::printf("  %d - %d: g_gameFinished after %d frames; pvp_win.ogg %s\n", Script::g_pvpPoints[0],
                Script::g_pvpPoints[1], won, g.sound.PlayedSince("pvp_win.ogg", matchStart) ? "played" : "NOT played");
    CHECK_EQ(Script::g_pvpPoints[0], Script::MAX_PVP_POINTS);
    CHECK(won >= 0);
    CHECK(g.sound.PlayedSince("pvp_win.ogg", matchStart));
    CHECK(WaitForHud(g, "Jogador 1 \xE9 o vencedor!", 3));
    g.Steps(60);
    CHECK(GetSceneFileName() == "scenes/pvp_lv6.esc");
    std::printf("-- the end screen's cancel: JK_09 (Back) on player 1's pad (setupScene.as:390)\n");
    InputFrame back = g.base;
    back.pads[1].buttons[static_cast<std::size_t>(JK_09)] = true;
    g.Step(back);
    const int menu = WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/menu.esc"; });
    std::printf("  menu.esc %d frames after the press\n", menu);
    CHECK(menu >= 0);
    g.base.pads[0].connected = false;
    g.base.pads[1].connected = false;
    g.Steps(5);
}

// === 20. A lone gamepad, from boot (E12 under the shipped defaults) ===================================
//
// A fresh runtime booted with only what the layer reports for a player with
// one gamepad who touches neither keyboard nor mouse, under the shipped
// settings: firstPadIsPlayer1 (E12) puts the pad on index 1, player 1's under
// g_controls 0 (playerInput.as:43-53); keyboardPlayer2 (E4) keeps index 0
// connected and idle for a keyboard second player. Game::warpCursor stands in
// for InputMapper::WarpCursor. The script module's globals are not reset by a
// new Machine (the original ran one module per process), so the switches the
// earlier scenarios touched are checked back at row 0 first.

// Player 1's pad this frame: the stick at `xy`, `buttons` down.
InputFrame PadFrame(const Game& g, const vector2& xy, std::initializer_list<J_KEY> buttons = {}) {
    InputFrame f = g.base;
    f.pads[1].xy = xy;
    for (const J_KEY b : buttons) f.pads[1].buttons[static_cast<std::size_t>(b)] = true;
    return f;
}

// One press of a button on player 1's pad: down for a frame, then up.
void PadPress(Game& g, const J_KEY button) {
    g.Step(PadFrame(g, vector2(0.0f), {button}));
    g.Step();
}

// The stick pushed toward `target` as a player would - full tilt when far,
// easing off within 5 px - until the cursor is within 2 px; then two frames
// with the stick at rest (the cursor entity trails the cursor by one,
// menu.as:238-240). Returns the frames taken, or -1.
int SteerTo(Game& g, const vector2& target, const uint limit) {
    for (uint i = 0; i < limit; ++i) {
        const vector2 d = target - g.base.cursor;
        const float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 2.0f) {
            g.Steps(2);
            return static_cast<int>(i);
        }
        const float tilt = len < 5.0f ? len / 5.0f : 1.0f;
        g.Step(PadFrame(g, d * (tilt / len)));
    }
    return -1;
}

void ScenarioGamepadOnly(Game& g) {
    std::printf("  devices: pad 1 (the gamepad, E12) and pad 0 (keyboard player 2, E4, idle); no key; the mouse "
                "resting at (%.0f,%.0f)\n",
                g.base.cursor.x, g.base.cursor.y);
    CHECK_EQ(Script::g_controls.getCurrent(), 0u);
    CHECK_EQ(Script::g_enablePS.getCurrent(), 0u);
    CHECK_EQ(Script::getPlayerJoystick(0), 1u);
    CHECK_EQ(Script::getPlayerJoystick(1), 0u);
    WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/menu.esc"; });
    CHECK(GetSceneFileName() == "scenes/menu.esc");
    g.Steps(30);
    CHECK(g.sound.Looping("menu.mp3"));
    // menu.as:79-88: an icon per detected joystick.
    CHECK(HudHasSprite(g.m, "joystick.png"));

    std::printf("-- the menu: Back does nothing\n");
    PadPress(g, JK_09);
    g.Steps(3);
    CHECK(GetSceneFileName() == "scenes/menu.esc");
    CHECK(SeekEntity("cursor.ent") != nullptr);

    std::printf("-- the stick moves the cursor, 5 px a frame at full tilt (menu.as:239)\n");
    const vector2 c0 = g.base.cursor;
    for (int i = 0; i < 10; ++i) g.Step(PadFrame(g, vector2(-1.0f, 0.0f)));
    const ETHEntity cursor = SeekEntity("cursor.ent");
    std::printf("  10 frames left: cursor (%.1f,%.1f) -> (%.1f,%.1f); cursor.ent at x %.1f\n", c0.x, c0.y,
                g.base.cursor.x, g.base.cursor.y, cursor != nullptr ? cursor->GetPosition().x : -1.0f);
    CHECK_NEAR(g.base.cursor.x, c0.x - 50.0f);
    CHECK_NEAR(g.base.cursor.y, c0.y);
    if (cursor != nullptr) CHECK_NEAR(cursor->GetPosition().x, c0.x - 45.0f);

    std::printf("-- to the options with the stick, Start, a row, Back\n");
    const int toOptions = SteerTo(g, kOptionsButton, 400);
    std::printf("  on '%s' after %d frames\n", LastButton().c_str(), toOptions);
    CHECK(toOptions >= 0);
    CHECK(LastButton() == "opcoes_de_video");
    PadPress(g, JK_10);
    const int options = WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/videoModes.esc"; });
    std::printf("  Start: videoModes.esc %d frames later\n", options);
    CHECK(options >= 0);
    g.Steps(3);
    // The picker follows the stick as the menu cursor does (videoModes.as:58-63).
    const int toRow = SteerTo(g, vector2(300.0f, 137.0f), 400);
    const ETHEntity picker = SeekEntity("picker");
    std::printf("  on 'Desativa pixel shaders' after %d frames; picker at (%.1f,%.1f)\n", toRow,
                picker != nullptr ? picker->GetPosition().x : -1.0f, picker != nullptr ? picker->GetPosition().y : -1.0f);
    CHECK(toRow >= 0);
    CHECK(picker != nullptr);
    if (picker != nullptr) {
        CHECK(std::fabs(picker->GetPosition().x - 300.0f) < 2.5f);
        CHECK(std::fabs(picker->GetPosition().y - 137.0f) < 2.5f);
    }
    PadPress(g, JK_10);
    std::printf("  Start: g_enablePS %u\n", Script::g_enablePS.getCurrent());
    CHECK_EQ(Script::g_enablePS.getCurrent(), 1u);
    SteerTo(g, vector2(300.0f, 112.0f), 400);
    PadPress(g, JK_10);
    CHECK_EQ(Script::g_enablePS.getCurrent(), 0u);
    PadPress(g, JK_09);
    const int backToMenu = WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/menu.esc"; });
    std::printf("  Back: menu.esc %d frames later\n", backToMenu);
    CHECK(backToMenu >= 0);
    g.Steps(10);

    std::printf("-- Versus: pad 0 (keyboard player 2) makes a second controller\n");
    SteerTo(g, kVersusButton, 400);
    CHECK(LastButton() == "versus");
    CHECK(Script::hasASecondController());
    CHECK(WaitForHud(g, "Escolha uma arena", 3));
    PadPress(g, JK_10);
    const int select = WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/arena_select.esc"; });
    std::printf("  Start: arena_select.esc %d frames later\n", select);
    CHECK(select >= 0);
    g.Steps(3);
    SteerTo(g, kArena1Thumbnail, 400);
    CHECK(LastButton() == "thumbnail");
    CHECK(WaitForHud(g, "Obelisco", 3));
    // Thumbnail "5" (Templo Sagrado) at (510,219), score 720000: this user
    // directory has no record, so the best time is the shipped hs.enml's 59:59
    // and the arena is locked - Start is refused with fail.ogg (menu.as:311-338).
    std::printf("  best time %u ms: Templo Sagrado (score 720000) locked\n", Script::getGetBestTime());
    SteerTo(g, vector2(510.0f, 251.0f), 400);
    CHECK(LastButton() == "thumbnail");
    CHECK(WaitForHud(g, "Templo Sagrado", 3));
    CHECK(HudHas(g.m, "liberar esta arena"));
    const uint locked = g.Frame() + 1;
    PadPress(g, JK_10);
    const ETHEntity selectCursor = SeekEntity("cursor.ent");
    std::printf("  Start on it: fail.ogg %s, newGame %s\n", g.sound.PlayedSince("fail.ogg", locked) ? "played" : "NOT played",
                (selectCursor != nullptr && selectCursor->CheckCustomData("newGame") != DT_NODATA) ? "SET" : "not set");
    CHECK(g.sound.PlayedSince("fail.ogg", locked));
    if (selectCursor != nullptr) CHECK(selectCursor->CheckCustomData("newGame") == DT_NODATA);
    CHECK(GetSceneFileName() == "scenes/arena_select.esc");
    PadPress(g, JK_09);
    const int fromSelect = WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/menu.esc"; });
    std::printf("  Back: menu.esc %d frames later\n", fromSelect);
    CHECK(fromSelect >= 0);
    g.Steps(10);

    std::printf("-- New game with Start\n");
    const int toNew = SteerTo(g, kNewGameButton, 400);
    std::printf("  on '%s' after %d frames\n", LastButton().c_str(), toNew);
    CHECK(LastButton() == "novo_jogo");
    const uint press = g.Frame() + 1;
    PadPress(g, JK_10);
    CHECK(g.sound.PlayedSince("newgame.mp3", press));
    const int loaded = WaitFor(g, 240, [] { return GetSceneFileName() == "scenes/level1.esc"; });
    std::printf("  level1.esc %d frames after the release\n", loaded);
    CHECK(loaded >= 178 && loaded <= 182);
    const ETHEntity p = WaitForPlayerReady(g, 240, "level1 by the pad");
    if (!Ready(p)) {
        CHECK_MSG(false, "no wizard");
        return;
    }

    std::printf("-- in the level the pad plays: stick, JK_03 jump, JK_04 sword\n");
    const float x0 = p->GetPosition().x;
    for (int i = 0; i < 30; ++i) g.Step(PadFrame(g, vector2(1.0f, 0.0f)));
    g.Step();
    std::printf("  30 frames of stick right: x %.1f -> %.1f, facing %u\n", x0, p->GetPosition().x,
                p->GetUIntData("currentDir"));
    CHECK(p->GetPosition().x - x0 > 50.0f);
    CHECK_EQ(p->GetUIntData("currentDir"), Script::RIGHT);
    WaitForPlayerReady(g, 60, "before the jump");
    const float y0 = p->GetPosition().y;
    PadPress(g, JK_03);
    float apex = y0;
    for (int i = 0; i < 40; ++i) {
        g.Step();
        apex = std::min(apex, p->GetPosition().y);
    }
    std::printf("  JK_03: %.1f px up\n", y0 - apex);
    CHECK(y0 - apex > 30.0f);
    WaitForPlayerReady(g, 60, "before the sword");
    int since = GetLastID();
    g.Step(PadFrame(g, vector2(0.0f), {JK_04}));
    const uint swords = static_cast<uint>(NewEntities("sword0.ent", since).size());
    g.Step();
    std::printf("  JK_04: sword0.ent %u\n", swords);
    CHECK_EQ(swords, 1u);

    std::printf("-- Start and Back do nothing in a level: no summon (player 2's Start does that), no way out\n");
    since = GetLastID();
    const uint setup = Script::g_levelStartTime;
    PadPress(g, JK_10);
    PadPress(g, JK_09);
    g.Steps(5);
    CHECK(NewEntities(kPrincess, since).empty());
    CHECK(GetSceneFileName() == "scenes/level1.esc");
    CHECK(Script::g_levelStartTime == setup);

    std::printf("-- game over, and Back to the menu (gameover.as:68)\n");
    // SHORTCUT: no lives left, and the wizard's hp set to 0.
    std::printf("  SHORTCUT: g_lives set to 0, wizard hp to 0\n");
    Script::g_lives = 0;
    p->AddIntData("hp", 0);
    const int over = WaitFor(g, 200, [] { return GetSceneFileName() == "scenes/gameover.esc"; });
    std::printf("  gameover.esc %d frames later\n", over);
    CHECK(over >= 180 && over <= 183);
    g.Steps(30);
    PadPress(g, JK_09);
    const int fromOver = WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/menu.esc"; });
    std::printf("  Back: menu.esc %d frames later\n", fromOver);
    CHECK(fromOver >= 0);
    g.Steps(10);

    std::printf("-- Quit with Start\n");
    SteerTo(g, kQuitButton, 400);
    CHECK(LastButton() == "sair");
    CHECK(!g.m.QuitRequested());
    PadPress(g, JK_10);
    CHECK(g.m.QuitRequested());

    std::printf("  player 1's pad (index 1), screen by screen:\n"
                "    menu.esc            confirm JK_10 (Start) on the button under the cursor; cancel none (JK_09 is "
                "read, and goToMenu does nothing on the menu)\n"
                "    videoModes.esc      confirm JK_10 on a row or on the back arrow; cancel JK_09 -> menu\n"
                "    arena_select.esc    confirm JK_10 on an unlocked thumbnail; cancel JK_09 -> menu\n"
                "    a level             neither: JK_10 is player 2's summon; the way out is ESC only "
                "(setupScene.as:392)\n"
                "    end screens         cancel JK_09 -> menu (campaign and Versus, setupScene.as:390)\n"
                "    gameover.esc        cancel JK_09 -> menu\n");
}

// === 21. The options screen on a phone (E20) ===============================================
//
// Script::g_mobileLayout is the layer's to raise, on a PENUMBRA_MOBILE build; here
// it is raised by hand, in a runtime of its own, so no scenario above ever sees
// it. With it down the screen is scenario 14's, and this checks that too: the
// video-mode list, the window switch and the Alt+Enter line are there, then gone,
// then back, and the touch controls' row stands where the window switch was.
void ScenarioMobileOptions(Game& g) {
    const char* const altEnter = "Pressione Alt+Enter para trocar entre fullscreen e modo janela";
    // One mode, so that the list's absence is a fact rather than an empty list.
    g.m.SetVideoModes({videoMode{1280, 720, PF32BIT}});
    CHECK(!Script::g_mobileLayout);
    CHECK(EnsureMenu(g));
    CHECK(WaitForHud(g, altEnter, 3));   // the menu's footer, down

    Script::g_mobileLayout = true;
    g.Steps(2);
    CHECK(!HudHas(g.m, altEnter));   // and up
    std::printf("  menu footer with the phone's layout: %s\n", HudHas(g.m, altEnter) ? "SHOWN" : "gone");

    g.base.cursor = kOptionsButton;
    g.Steps(3);
    CHECK(LastButton() == "opcoes_de_video");
    g.Step(g.With({K_RETURN}));
    CHECK(WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/videoModes.esc"; }) >= 0);
    g.Steps(3);

    CHECK(WaitForHud(g, "[\x95] Ativa controles de toque", 3));
    CHECK(HudHas(g.m, "[ ] Desativa controles de toque"));
    CHECK(!HudHas(g.m, "Janela"));
    CHECK(!HudHas(g.m, "Tela-cheia"));
    CHECK(!HudHas(g.m, "1280x720x32"));
    CHECK(!HudHas(g.m, altEnter));
    // Everything else is where it was.
    CHECK(HudHas(g.m, "[\x95] Ativa pixel shaders"));
    CHECK(HudHas(g.m, "Teclado para o jogador 2"));
    CHECK(HudHas(g.m, "Pausa ao perder o foco"));

    const auto click = [&g](const vector2& at) {
        g.base.cursor = at;
        g.Steps(2);
        g.Step(g.With({K_RETURN}));
        g.Steps(2);
    };
    // Two 25 px rows 256 wide from (255, 170), where g_windowed's are.
    const unsigned windowedBefore = Script::g_windowed.getCurrent();
    CHECK_EQ(Script::g_touchControls.getCurrent(), 0u);
    click(vector2(300.0f, 207.0f));
    std::printf("  touch controls' second row clicked: switch %u\n", Script::g_touchControls.getCurrent());
    CHECK_EQ(Script::g_touchControls.getCurrent(), 1u);
    CHECK(WaitForHud(g, "[\x95] Desativa controles de toque", 3));
    CHECK(HudHas(g.m, "[ ] Ativa controles de toque"));
    click(vector2(300.0f, 182.0f));
    CHECK_EQ(Script::g_touchControls.getCurrent(), 0u);
    CHECK(WaitForHud(g, "[\x95] Ativa controles de toque", 3));
    CHECK_EQ(Script::g_windowed.getCurrent(), windowedBefore);   // the hidden switch did not move

    // Down again: the desktop's screen, from the next frame.
    Script::g_mobileLayout = false;
    g.Steps(2);
    CHECK(HudHas(g.m, "Janela"));
    CHECK(HudHas(g.m, "1280x720x32"));
    CHECK(HudHas(g.m, altEnter));
    CHECK(!HudHas(g.m, "controles de toque"));

    g.Step(g.With({K_ESC}));
    CHECK(WaitFor(g, 3, [] { return GetSceneFileName() == "scenes/menu.esc"; }) >= 0);
    g.Steps(5);
}

} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (!std::filesystem::exists(PENUMBRA_ORIGINAL_DIR "/scenes/level1.esc")) {
        std::printf("SKIP: the original is not at %s\n", PENUMBRA_ORIGINAL_DIR);
        return 77;
    }

    // A fresh user directory: hs.enml and scenes/checkpoint.esc start as a
    // first launch's would.
    std::error_code ec;
    const std::filesystem::path userRoot =
        std::filesystem::temp_directory_path(ec) / ("penumbra-scenarios-" + std::to_string(std::random_device{}()));
    std::filesystem::remove_all(userRoot, ec);
    std::filesystem::create_directories(userRoot, ec);
    const string originalHsBefore = ReadFile(std::filesystem::path(PENUMBRA_ORIGINAL_DIR) / "hs.enml");

    {
        MachineConfig config;
        config.userRoot = userRoot.generic_string();
        Machine machine(config);
        Machine::Scope scope(machine);
        SoundLog sound;
        // Before Boot: menuPreLoop loads and loops the menu music on frame 1.
        machine.Samples().SetOutput(&sound);
        Script::RegisterAll(machine);
        machine.Boot(Script::ScriptMain);

        Game g{machine, sound, userRoot.generic_string(), InputFrame{}};
        g.Step();
        Trace(g, "boot");

        RunScenario(g, "10. menu navigation", ScenarioMenu);
        RunScenario(g, "1. enemy melee", ScenarioMelee);
        RunScenario(g, "7. potion", ScenarioPotion);
        RunScenario(g, "6a. lava shooter and instant death", ScenarioShooterAndLava);
        RunScenario(g, "3. checkpoint", ScenarioCheckpoint);
        RunScenario(g, "5. level transition", ScenarioNextLevel);
        RunScenario(g, "2. ranged enemy and fire resistance", ScenarioImpy);
        RunScenario(g, "4. deaths without a checkpoint and game over", ScenarioGameOver);
        RunScenario(g, "6b+8. falling bridge, the king, the end screen", ScenarioBridgeAndBoss);
        RunScenario(g, "9. versus", ScenarioPvp);
        RunScenario(g, "11. double jump, light, combos, a knight", ScenarioMoves);
        RunScenario(g, "12. co-op: the summoned princess", ScenarioCoop);
        RunScenario(g, "14. the options screen's enhanced rows (E10)", ScenarioOptionsE10);   // before the Quit
        // The paths no scenario above takes, appended so that none above
        // changes (every added frame would reshuffle the later random rolls).
        RunScenario(g, "15. level2 by the K_2 cheat; the play_sound markers", ScenarioPlaySound);
        RunScenario(g, "16. level2 -> level3 through next_level", ScenarioLevel2ToLevel3);
        RunScenario(g, "17. the paladin and the master knight", ScenarioPaladinAndMasterKnight);
        RunScenario(g, "18. the summon's price and its refusals", ScenarioSummonRules);
        RunScenario(g, "19. versus in arenas 2-6, arena 6 to 3 points", ScenarioArenas);
        RunScenario(g, "13. the menu's Quit", [](Game& game) {
            CHECK(EnsureMenu(game));
            game.base.cursor = kQuitButton;
            game.Steps(3);
            std::printf("  cursor over '%s'\n", LastButton().c_str());
            CHECK(LastButton() == "sair");
            CHECK(!game.m.QuitRequested());
            game.Step(game.With({K_RETURN}));
            std::printf("  Enter: quit requested %s\n", game.m.QuitRequested() ? "yes" : "NO");
            CHECK(game.m.QuitRequested());
        });

        std::printf("\n=== summary (frame %u)\n", machine.FrameIndex());
        for (const Result& r : g_results) {
            std::printf("  %-50s %s  %d failed checks, %u aborts%s\n", r.name.c_str(),
                        (r.failures == 0 && r.aborts == 0 && !r.threw) ? "PASS" : "FAIL", r.failures, r.aborts,
                        r.threw ? ", threw" : "");
        }
        std::printf("  script aborts: %u\n", machine.ScriptAborts());
        for (const string& site : machine.AbortSites()) std::printf("    ABORT %s\n", Utf8(site).c_str());
        std::printf("  sound files that failed to load: %zu\n", sound.failedLoads.size());
        for (const string& f : sound.failedLoads) std::printf("    %s\n", f.c_str());
        CHECK(sound.failedLoads.empty());
        CHECK_EQ(machine.ScriptAborts(), 0u);
    }

    // 20: a second runtime, booted for a player with one gamepad.
    {
        const std::filesystem::path padRoot = userRoot / "pad-only";
        std::filesystem::create_directories(padRoot, ec);
        SoundLog sound;
        MachineConfig config;
        config.userRoot = padRoot.generic_string();
        Machine machine(config);
        Machine::Scope scope(machine);
        machine.Samples().SetOutput(&sound);
        Script::RegisterAll(machine);
        machine.Boot(Script::ScriptMain);

        Game g{machine, sound, padRoot.generic_string(), InputFrame{}};
        g.warpCursor = true;
        g.base.cursor = vector2(900.0f, 700.0f);
        g.base.cursorAbsolute = g.base.cursor;
        g.base.pads[0].connected = true;   // keyboard player 2 (E4), idle
        g.base.pads[1].connected = true;   // the one gamepad (E12)
        RunScenario(g, "20. a lone gamepad from boot (E12)", ScenarioGamepadOnly);
        const Result& r = g_results.back();
        std::printf("\n=== second runtime (frame %u)\n  %-50s %s  %d failed checks, %u aborts%s\n",
                    machine.FrameIndex(), r.name.c_str(), (r.failures == 0 && r.aborts == 0 && !r.threw) ? "PASS" : "FAIL",
                    r.failures, r.aborts, r.threw ? ", threw" : "");
        for (const string& site : machine.AbortSites()) std::printf("    ABORT %s\n", Utf8(site).c_str());
        for (const string& f : sound.failedLoads) std::printf("    failed to load %s\n", f.c_str());
        CHECK(sound.failedLoads.empty());
        CHECK_EQ(machine.ScriptAborts(), 0u);
    }

    // 21: a third runtime, for the phone's options screen (E20), so that the
    // flag it raises reaches none of the scenarios above.
    {
        const std::filesystem::path phoneRoot = userRoot / "phone";
        std::filesystem::create_directories(phoneRoot, ec);
        SoundLog sound;
        MachineConfig config;
        config.userRoot = phoneRoot.generic_string();
        Machine machine(config);
        Machine::Scope scope(machine);
        machine.Samples().SetOutput(&sound);
        Script::RegisterAll(machine);
        machine.Boot(Script::ScriptMain);

        Game g{machine, sound, phoneRoot.generic_string(), InputFrame{}};
        g.Step();
        RunScenario(g, "21. the options screen on a phone (E20)", ScenarioMobileOptions);
        Script::g_mobileLayout = false;   // whatever the scenario reached
        const Result& r = g_results.back();
        std::printf("\n=== third runtime (frame %u)\n  %-50s %s  %d failed checks, %u aborts%s\n",
                    machine.FrameIndex(), r.name.c_str(), (r.failures == 0 && r.aborts == 0 && !r.threw) ? "PASS" : "FAIL",
                    r.failures, r.aborts, r.threw ? ", threw" : "");
        for (const string& site : machine.AbortSites()) std::printf("    ABORT %s\n", Utf8(site).c_str());
        CHECK(sound.failedLoads.empty());
        CHECK_EQ(machine.ScriptAborts(), 0u);
    }

    CHECK(ReadFile(std::filesystem::path(PENUMBRA_ORIGINAL_DIR) / "hs.enml") == originalHsBefore);
    CHECK(!FileExists(std::filesystem::path(PENUMBRA_ORIGINAL_DIR) / "scenes" / "checkpoint.esc"));
    std::filesystem::remove_all(userRoot, ec);
    return test::summary("test_pn_scenarios", 400);
}
