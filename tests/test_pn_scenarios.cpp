// The REAL ported game, played headless past level 1's first room: the menu,
// a melee fight, a ranged fire-resistant enemy, a potion, the hazards, a
// checkpoint and a death that returns to it, the level exit, the lives running
// out into the game-over screen, the king and the end screen with its high
// score, a Versus match to three points; then the wizard's other moves (double
// jump, light, both combos) against a knight, the co-op creature summoned by a
// second controller, the options screen's enhanced rows (E10), and the menu's
// Quit.
//
// The harness is test_pn_boot's: one Machine (the script module's globals live
// for the whole program, as they lived for the whole of machine.exe),
// RegisterAll, Boot(ScriptMain), one Frame per InputFrame. Everything happens
// through the game's own logic. Where reaching a situation by playing would
// take minutes, the suite takes a SHORTCUT, and each one is named where it is
// taken, in the printed trace and in a comment:
//   - teleporting the wizard or the princess (SetPosition, forces zeroed);
//   - moving a spawn MARKER next to the wizard, so that doLoop spawns the enemy
//     from it exactly as it would have when the camera reached it;
//   - writing a custom datum the game itself writes (hp, mp), e.g. "the wizard
//     is not at full health", "the king has lost his last hit point".
// Nothing else is forced: AI, damage, deaths, fades, loads, saves are the
// game's.
//
// The chain of scenes is the game's own: menu -> level1 -> checkpoint ->
// level2 -> game over -> menu -> level3 (the original's K_3 cheat) -> end
// screen -> menu -> arena select -> pvp_lv1 -> menu -> level1. Each scenario
// still starts from a known state and, if the previous one did not leave it
// there, gets there by the game's own entry point (LoadScene with setupScene,
// newGame).
//
// Findings about the ORIGINAL that the suite pins rather than fixes: the
// checkpoint save holds the checkpoint itself, so a respawn takes it again
// (main.as:184-186); and the walk cycle restarts where two floor tiles meet,
// because doCharacterCollision keeps only the last collided box's thinner-box
// test (controlCharacters.as:126), leaving the wizard "in the air" for a frame.
//
// A sound recorder stands in for the speakers: every sample the game plays is
// logged with its frame and loop flag (see SoundLog for how long a one-shot
// "plays").

#include "script/Script.hpp"

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

    void Step(const InputFrame& input) { m.Frame(input); }
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
        float y;
        const char* row0;
        const char* row1;
    };
    const E10Switch e10Switches[] = {
        {&Script::g_keyboardP2, 424.0f, "Teclado para o jogador 2", "Jogador 2 s\xF3 no joystick"},
        {&Script::g_widescreen, 494.0f, "Tela larga (widescreen)", "Tela 4:3 (original)"},
        {&Script::g_language, 564.0f, "Portugu\xEAs", "English"},
    };
    for (const E10Switch& row : e10Switches) {
        CHECK_EQ(row.widget->getCurrent(), 0u);
        click(vector2(300.0f, row.y + 37.0f));
        std::printf("  '%s': switch %u\n", Utf8(row.row1).c_str(), row.widget->getCurrent());
        CHECK_EQ(row.widget->getCurrent(), 1u);
        CHECK(WaitForHud(g, string("[\x95] ") + row.row1, 3));
        CHECK(HudHas(g.m, string("[ ] ") + row.row0));
        click(vector2(300.0f, row.y + 12.0f));
        CHECK_EQ(row.widget->getCurrent(), 0u);
        CHECK(WaitForHud(g, string("[\x95] ") + row.row0, 3));
    }
    // None of them moved the original's switches.
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

    CHECK(ReadFile(std::filesystem::path(PENUMBRA_ORIGINAL_DIR) / "hs.enml") == originalHsBefore);
    CHECK(!FileExists(std::filesystem::path(PENUMBRA_ORIGINAL_DIR) / "scenes" / "checkpoint.esc"));
    std::filesystem::remove_all(userRoot, ec);
    return test::summary("test_pn_scenarios", 400);
}
