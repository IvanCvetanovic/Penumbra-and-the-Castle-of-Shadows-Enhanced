// The REAL ported game, booted headless on the Eth Machine: main() (here
// ScriptMain) loads the menu, the menu's newGame("CAMPAIGN") loads level1,
// doLoop spawns the wizard from his marker, and keyboard frames walk, jump and
// swing the sword. Nothing is drawn; the snapshot is only counted.
//
// This is the integration check the other suites cannot be: every ported
// script file is linked and runs against the runtime, the scenes and the
// definitions of the original (read in place from extracted/app). A
// ScriptException anywhere is a porting bug, so the run ends by printing every
// site that aborted.
//
// One Machine per process: the script module's globals (g_camera, g_lives...)
// live for the whole program, as they lived for the whole of machine.exe.

#include "script/Script.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>

#include "TestHarness.hpp"

using namespace Penumbra::Eth;
namespace Script = Penumbra::Script;

namespace {

// The wizard (bruxo.ent: speed 150, jumpForce 507) as doLoop spawns him from
// level1's marker id 55 at (410,-42) (setupScene.as:270, controlCharacters.as:270).
constexpr const char* kPlayer = "bruxo.ent";

struct Run {
    Machine& machine;
    uint frame = 0;

    void Step(const InputFrame& input) {
        machine.Frame(input);
        ++frame;
    }
    void Step() { Step(InputFrame{}); }
};

ETHEntity Player() { return SeekEntity(kPlayer); }

bool OnGround(const ETHEntity& player) { return player != nullptr && player->GetUIntData("touchingGround") != 0; }

uint CountAlive(const string& name) {
    ETHEntityArray found;
    GetEntityArray(name, found);
    uint n = 0;
    for (const ETHEntity& e : found) {
        if (e->IsAlive()) ++n;
    }
    return n;
}

void Trace(const Run& run, const char* what) {
    const Machine& m = run.machine;
    const RenderSnapshot& snap = m.Snapshot();
    const ETHEntity player = Player();
    const vector2 cam = GetCameraPos();
    if (player != nullptr) {
        const vector3 p = player->GetPosition();
        std::printf("  [%4u] %-14s %-20s ents %4u  bruxo (%8.2f, %8.2f) ground %u  cam (%7.1f, %7.1f)  "
                    "sprites %3zu lights %2zu particles %3zu hud %2zu  aborts %u\n",
                    run.frame, what, GetSceneFileName().c_str(), GetNumEntities(), p.x, p.y,
                    player->GetUIntData("touchingGround"), cam.x, cam.y, snap.sprites.size(), snap.lights.size(),
                    snap.particles.size(), snap.hud.size(), m.ScriptAborts());
    } else {
        std::printf("  [%4u] %-14s %-20s ents %4u  (no bruxo)  cam (%7.1f, %7.1f)  sprites %3zu lights %2zu "
                    "particles %3zu hud %2zu  aborts %u\n",
                    run.frame, what, GetSceneFileName().c_str(), GetNumEntities(), cam.x, cam.y,
                    snap.sprites.size(), snap.lights.size(), snap.particles.size(), snap.hud.size(),
                    m.ScriptAborts());
    }
}

void PrintAborts(const Machine& m) {
    std::printf("  script aborts: %u\n", m.ScriptAborts());
    for (const string& site : m.AbortSites()) std::printf("    ABORT %s\n", site.c_str());
}

// Frames until the player exists and stands, at most `limit`.
bool WaitForGround(Run& run, const uint limit, const char* what) {
    for (uint i = 0; i < limit; ++i) {
        if (OnGround(Player())) return true;
        run.Step();
    }
    const bool ok = OnGround(Player());
    if (!ok) std::printf("  %s: bruxo not on the ground after %u frames\n", what, limit);
    return ok;
}

void TestMenu(Run& run) {
    std::printf("-- boot and menu\n");
    run.Step();
    Trace(run, "frame 1");
    CHECK(GetSceneFileName() == "scenes/menu.esc");
    run.Step();
    CHECK(GetSceneFileName() == "scenes/menu.esc");
    // menu.esc: the cursor, the buttons (bare labels) and the scenery.
    CHECK(SeekEntity("cursor.ent") != nullptr);
    CHECK(SeekEntity("novo_jogo") != nullptr);
    CHECK(SeekEntity("gamelogo.ent") != nullptr);
    // menu.esc places 69 entities, and the menu adds none while idle.
    CHECK_EQ(GetNumEntities(), 69u);
    // menuPreLoop wrote these onto the cursor (menu.as:66-70).
    const ETHEntity cursor = SeekEntity("cursor.ent");
    if (cursor != nullptr) CHECK(cursor->GetStringData("lastButton") == "none");
    for (int i = 0; i < 58; ++i) run.Step();
    Trace(run, "menu idle");
    CHECK(GetSceneFileName() == "scenes/menu.esc");
    CHECK(!run.machine.Snapshot().sprites.empty());
    // main.as:136-137: lives from data.enml.
    CHECK_EQ(Script::g_lives, 13);
    CHECK_EQ(run.machine.ScriptAborts(), 0u);
}

// Returns the player's handle once level1 runs with him spawned.
ETHEntity TestLevelStart(Run& run) {
    std::printf("-- newGame(\"CAMPAIGN\") -> level1\n");
    // The menu calls it from the cursor callback when the fade-out ends
    // (menu.as:352-354, scene "CAMPAIGN" on novo_jogo); here it is called
    // directly, outside the Machine's guard, so an abort is caught here.
    const uint calledAt = run.frame;
    try {
        Script::newGame("CAMPAIGN");
    } catch (const ScriptException& e) {
        std::printf("  newGame threw: %s\n", e.what());
        CHECK_MSG(false, "newGame threw a ScriptException");
    }
    uint spawnedAt = 0;
    for (uint i = 0; i < 240; ++i) {
        run.Step();
        if (spawnedAt == 0 && Player() != nullptr) {
            spawnedAt = run.frame;
            Trace(run, "spawned");
        }
        if (i % 40 == 39) Trace(run, "level1");
    }
    CHECK(GetSceneFileName() == "scenes/level1.esc");
    const ETHEntity player = Player();
    CHECK(player != nullptr);
    if (player == nullptr) return player;
    std::printf("  bruxo spawned on frame %u after newGame\n", spawnedAt - calledAt);
    CHECK(player->IsAlive());
    CHECK_EQ(player->GetUIntData("playerId"), 0u);
    CHECK(player->GetIntData("hp") > 0);
    // The marker is gone and the wizard stands where it was (x 410).
    CHECK(std::fabs(player->GetPosition().x - 410.0f) < 2.0f);
    // The camera follows him: he is on screen.
    const vector2 onScreen = player->GetPositionXY() - GetCameraPos();
    CHECK(onScreen.x > 0.0f && onScreen.x < 1024.0f && onScreen.y > 0.0f && onScreen.y < 768.0f);
    CHECK(GetCameraPos() != vector2(0.0f));
    const RenderSnapshot& snap = run.machine.Snapshot();
    CHECK(snap.sprites.size() > 20u);
    CHECK(!snap.lights.empty());
    // drawPlayerStatus's bars and the timer, queued by the callbacks and the loop.
    CHECK(!snap.hud.empty());
    CHECK(snap.sceneFile == "scenes/level1.esc");
    CHECK_EQ(run.machine.ScriptAborts(), 0u);
    return player;
}

void TestWalk(Run& run, const ETHEntity& player) {
    std::printf("-- hold K_RIGHT for 120 frames\n");
    CHECK(WaitForGround(run, 120, "walk"));
    const vector3 start = player->GetPosition();
    const vector2 camStart = GetCameraPos();
    InputFrame right;
    right.keys[K_RIGHT] = true;
    for (uint i = 0; i < 120; ++i) {
        run.Step(right);
        if (i % 20 == 19) Trace(run, "right");
    }
    run.Step();   // release
    const vector3 end = player->GetPosition();
    std::printf("  x %.2f -> %.2f (%.2f px in 120 frames; 150 px/s would be 300)\n", start.x, end.x, end.x - start.x);
    CHECK(end.x - start.x > 100.0f);
    CHECK_EQ(player->GetUIntData("currentDir"), Script::RIGHT);
    // The camera moved with him and he is still on screen.
    const vector2 onScreen = player->GetPositionXY() - GetCameraPos();
    std::printf("  camera x %.1f -> %.1f\n", camStart.x, GetCameraPos().x);
    CHECK(GetCameraPos().x > camStart.x);
    CHECK(onScreen.x > 0.0f && onScreen.x < 1024.0f);
    CHECK_EQ(run.machine.ScriptAborts(), 0u);
}

void TestJump(Run& run, const ETHEntity& player) {
    std::printf("-- press K_UP once\n");
    CHECK(WaitForGround(run, 120, "jump"));
    for (int i = 0; i < 10; ++i) run.Step();
    const float y0 = player->GetPosition().y;
    InputFrame up;
    up.keys[K_UP] = true;
    run.Step(up);   // KS_HIT on this frame's input update
    float minY = y0;
    uint airFrames = 0;
    uint landedAt = 0;
    for (uint i = 0; i < 120; ++i) {
        run.Step();
        const float y = player->GetPosition().y;
        if (y < minY) minY = y;
        if (!OnGround(player)) ++airFrames;
        else if (landedAt == 0 && airFrames > 0) landedAt = i + 1;
        if (i % 10 == 9 && landedAt == 0) Trace(run, "jump");
    }
    Trace(run, "after jump");
    std::printf("  y %.2f, apex %.2f (%.2f px up), %u frames in the air, landed %u frames after the press\n", y0,
                minY, y0 - minY, airFrames, landedAt);
    CHECK(y0 - minY > 30.0f);
    CHECK(airFrames > 10u);
    CHECK(landedAt > 0u);
    CHECK(OnGround(player));
    CHECK(std::fabs(player->GetPosition().y - y0) < 2.0f);
    CHECK_EQ(player->GetIntData("jumps"), 0);
    CHECK_EQ(run.machine.ScriptAborts(), 0u);
}

void TestSword(Run& run, const ETHEntity& player) {
    std::printf("-- press K_S once\n");
    CHECK(WaitForGround(run, 60, "sword"));
    // Well past the sword cooldown and any combo window (combo.as:44).
    for (int i = 0; i < 30; ++i) run.Step();
    CHECK_EQ(CountAlive("sword0.ent"), 0u);
    InputFrame s;
    s.keys[K_S] = true;
    run.Step(s);
    const uint afterPress = CountAlive("sword0.ent");
    uint firstSeen = afterPress > 0 ? 1 : 0;
    uint lastSeen = firstSeen;
    for (uint i = 2; i <= 60; ++i) {
        run.Step();
        if (CountAlive("sword0.ent") > 0) {
            if (firstSeen == 0) firstSeen = i;
            lastSeen = i;
        }
    }
    std::printf("  sword0.ent alive from frame %u to frame %u after the press (%u frames)\n", firstSeen, lastSeen,
                firstSeen > 0 ? lastSeen - firstSeen + 1 : 0);
    CHECK(firstSeen >= 1 && firstSeen <= 2);
    CHECK(lastSeen >= 10 && lastSeen <= 25);
    CHECK_EQ(CountAlive("sword0.ent"), 0u);
    CHECK(player->GetUIntData("lastSwordAttack") != 0u);
    CHECK_EQ(run.machine.ScriptAborts(), 0u);
}

} // namespace

int main() {
    if (!std::filesystem::exists(PENUMBRA_ORIGINAL_DIR "/scenes/level1.esc")) {
        std::printf("SKIP: the original is not at %s\n", PENUMBRA_ORIGINAL_DIR);
        return 77;
    }

    // A fresh user directory, so hs.enml and checkpoint.esc start as a first
    // launch's would, and nothing of an earlier run is read back.
    std::error_code ec;
    const std::filesystem::path userRoot =
        std::filesystem::temp_directory_path(ec) / ("penumbra-boot-" + std::to_string(std::random_device{}()));
    std::filesystem::remove_all(userRoot, ec);
    std::filesystem::create_directories(userRoot, ec);

    {
        MachineConfig config;
        config.userRoot = userRoot.generic_string();
        Machine machine(config);
        Machine::Scope scope(machine);
        // Callbacks bind when an entity is created, so they must be registered
        // before the first scene loads.
        Script::RegisterAll(machine);
        machine.Boot(Script::ScriptMain);

        Run run{machine};
        TestMenu(run);
        const ETHEntity player = TestLevelStart(run);
        if (player != nullptr) {
            TestWalk(run, player);
            TestJump(run, player);
            TestSword(run, player);
        }
        Trace(run, "end");
        PrintAborts(machine);
        CHECK_EQ(machine.ScriptAborts(), 0u);
    }

    std::filesystem::remove_all(userRoot, ec);
    return test::summary("test_pn_boot", 46);
}
