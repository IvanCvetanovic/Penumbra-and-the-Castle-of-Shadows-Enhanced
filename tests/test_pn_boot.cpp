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
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>
#include <utility>

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

// === ENHANCEMENT E36: Normal and Hard =====================================================================

// Frames until setupScene has run again - a level, an arena or a checkpoint was loaded - at most `limit`.
// (A scene's name does not change when the same scene is loaded again.)
bool WaitForSetup(Run& run, const uint limit) {
    const uint before = Script::g_levelStartTime;
    for (uint i = 0; i < limit && Script::g_levelStartTime == before; ++i) run.Step();
    return Script::g_levelStartTime != before;
}

// An enemy as the scripts add one and give it its stats: AddEntity, then spawn(), with no frame between.
ETHEntity AddSpawned(const string& name, const ETHEntity& beside) {
    ETHEntity handle;
    AddEntity(name + ".ent", beside->GetPosition() + vector3(0.0f, 0.0f, -10.0f), handle);
    if (handle != nullptr) Script::spawn(handle, name);
    return handle;
}

// A spawn marker of `name` still on the map, moved 120 px to the wizard's right: doLoop spawns the enemy a frame
// later, as it does once the camera reaches the marker. The new entity; null when none came.
ETHEntity SpawnFromMarker(Run& run, const string& name, const ETHEntity& player) {
    ETHEntityArray before;
    GetEntityArray(name + ".ent", before);
    for (const ETHEntity& marker : Script::g_spawn) {
        if (!marker->IsAlive() || marker->GetStringData("name") != name) continue;
        const vector2 at = player->GetPositionXY();
        marker->SetPosition(vector3(at.x + 120.0f, at.y - 20.0f, 0.0f));
        for (int i = 0; i < 3; ++i) {
            run.Step();
            ETHEntityArray after;
            GetEntityArray(name + ".ent", after);
            for (const ETHEntity& found : after) {
                bool known = false;
                for (const ETHEntity& old : before) known = known || old->GetID() == found->GetID();
                if (!known) return found;
            }
        }
        return nullptr;
    }
    return nullptr;
}

// The lines FontAtlas sets a text in: CR LF is one break, a lone CR and an LF are one each.
uint LineCount(const string& text) {
    uint lines = 1;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') continue;
        if (text[i] == '\r' || text[i] == '\n') ++lines;
    }
    return lines;
}

string ReadUserHs(const std::filesystem::path& userRoot) {
    std::ifstream in(userRoot / "hs.enml", std::ios::binary);
    return string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// spawn(): data.enml's hp in Normal, twice it in Hard; the experience a kill gives is the base hp in both.
void TestDifficultyStats(Run& run, const ETHEntity& player) {
    std::printf("-- E36: spawn() gives an enemy data.enml's hp in Normal and twice it in Hard\n");
    CHECK_EQ(Script::g_runDifficulty, Script::DIFFICULTY_NORMAL);   // newGame("CAMPAIGN") with g_difficulty at Normal
    CHECK_EQ(Script::g_difficulty.getCurrent(), Script::DIFFICULTY_NORMAL);
    const auto stats = [&player](const string& name, const uint difficulty) {
        Script::g_runDifficulty = difficulty;
        const ETHEntity enemy = AddSpawned(name, player);
        std::pair<int, int> out(-1, -1);
        if (enemy != nullptr) {
            out = {enemy->GetIntData("hp"), enemy->GetIntData("expGiven")};
            DeleteEntity(enemy);
        }
        Script::g_runDifficulty = Script::DIFFICULTY_NORMAL;
        return out;
    };
    // The numbers as data.enml has them: a warrior 75, the king 3500, a minion 45.
    CHECK(stats("warrior", Script::DIFFICULTY_NORMAL) == std::make_pair(75, 75));
    CHECK(stats("warrior", Script::DIFFICULTY_HARD) == std::make_pair(150, 75));
    CHECK(stats("king", Script::DIFFICULTY_NORMAL) == std::make_pair(3500, 3500));
    CHECK(stats("king", Script::DIFFICULTY_HARD) == std::make_pair(7000, 3500));
    CHECK(stats("minion", Script::DIFFICULTY_NORMAL) == std::make_pair(45, 45));
    CHECK(stats("minion", Script::DIFFICULTY_HARD) == std::make_pair(90, 45));
    // Every enemy section of data.enml, whatever its hp.
    for (const char* name : {"warrior", "minion", "knight", "master_knight", "impy", "paladin", "king"}) {
        int base = 0;
        CHECK_MSG(Script::g_gameData.getInt(name, "hp", base) && base > 0, name);
        CHECK_MSG(stats(name, Script::DIFFICULTY_NORMAL) == std::make_pair(base, base), name);
        CHECK_MSG(stats(name, Script::DIFFICULTY_HARD) == std::make_pair(base * 2, base), name);
    }
    CHECK_EQ(Script::HARD_HP_FACTOR, 2u);

    // The same through a spawn marker, which is how a level's enemies come: doLoop -> spawn. Normal: untouched.
    const ETHEntity warrior = SpawnFromMarker(run, "warrior", player);
    CHECK(warrior != nullptr);
    if (warrior != nullptr) {
        CHECK_EQ(warrior->GetIntData("hp"), 75);
        CHECK_EQ(warrior->GetIntData("expGiven"), 75);
        DeleteEntity(warrior);
    }
}

// A Hard campaign run from New Game on: the latch, a marker's enemy, what a death and a checkpoint reload do to
// it, then Versus under the same g_difficulty, which stays Normal.
void TestHardRun(Run& run) {
    std::printf("-- E36: a Hard run (newGame latches g_difficulty, which New Game's prompt sets), a death, a checkpoint, then Versus\n");
    Script::g_difficulty.setCurrent(Script::DIFFICULTY_HARD);
    Script::newGame("CAMPAIGN");
    CHECK_EQ(Script::g_runDifficulty, Script::DIFFICULTY_HARD);
    CHECK(WaitForSetup(run, 10));
    CHECK(WaitForGround(run, 240, "hard level1"));
    ETHEntity player = Player();
    CHECK(player != nullptr);
    // g_difficulty is read once, when the run starts: changing it later leaves the run as it is.
    Script::g_difficulty.setCurrent(Script::DIFFICULTY_NORMAL);
    CHECK_EQ(Script::g_runDifficulty, Script::DIFFICULTY_HARD);
    if (player == nullptr) return;

    // A potion's hp (20) is what it heals: it is no enemy's, and no difficulty touches it.
    ETHEntityArray potions;
    GetEntityArray("potion_small.ent", potions);
    CHECK(!potions.empty());
    for (const ETHEntity& potion : potions) CHECK_EQ(potion->GetIntData("hp"), 20);

    ETHEntity warrior = SpawnFromMarker(run, "warrior", player);
    CHECK(warrior != nullptr);
    if (warrior != nullptr) {
        CHECK_EQ(warrior->GetIntData("hp"), 150);
        CHECK_EQ(warrior->GetIntData("expGiven"), 75);   // the experience a kill gives is the base hp
        for (int i = 0; i < 60; ++i) run.Step();
        CHECK_EQ(warrior->GetIntData("hp"), 150);        // nothing doubles it again as the level runs
        DeleteEntity(warrior);
    }

    // A death reloads the level without resetData: the run stays Hard, and the markers spawn Hard enemies afresh.
    const int lives = Script::g_lives;
    player->AddIntData("hp", 0);
    CHECK(WaitForSetup(run, 260));
    CHECK_EQ(Script::g_lives, lives - 1);
    CHECK_EQ(Script::g_runDifficulty, Script::DIFFICULTY_HARD);
    CHECK(WaitForGround(run, 240, "level1 after the death"));
    player = Player();
    CHECK(player != nullptr);
    if (player == nullptr) return;
    warrior = SpawnFromMarker(run, "warrior", player);
    CHECK(warrior != nullptr);
    if (warrior != nullptr) CHECK_EQ(warrior->GetIntData("hp"), 150);

    // A checkpoint file holds the enemy as it is (150): loading it spawns nothing and doubles nothing.
    if (warrior != nullptr) {
        const int id = warrior->GetID();
        const int hpSaved = warrior->GetIntData("hp");
        CHECK(SaveScene("scenes/checkpoint.esc"));
        LoadScene("scenes/checkpoint.esc", "setupScene", "levelLoop");
        CHECK(WaitForSetup(run, 10));
        for (int i = 0; i < 20; ++i) run.Step();
        CHECK(GetSceneFileName() == "scenes/checkpoint.esc");
        CHECK_EQ(Script::g_runDifficulty, Script::DIFFICULTY_HARD);
        const ETHEntity restored = SeekEntity(id);
        CHECK(restored != nullptr);
        if (restored != nullptr) {
            CHECK_EQ(restored->GetIntData("hp"), hpSaved);
            CHECK_EQ(restored->GetIntData("expGiven"), 75);
        }
        ETHEntityArray warriors;
        GetEntityArray("warrior.ent", warriors);
        for (const ETHEntity& each : warriors) CHECK(!each->IsAlive() || each->GetIntData("hp") <= 150);
    }

    // Versus is never Hard, whatever g_difficulty says: the arena select (resetData) and an arena (newGame).
    Script::g_difficulty.setCurrent(Script::DIFFICULTY_HARD);
    Script::goToPvp();
    CHECK_EQ(Script::g_runDifficulty, Script::DIFFICULTY_NORMAL);
    for (int i = 0; i < 3; ++i) run.Step();
    CHECK(GetSceneFileName() == "scenes/arena_select.esc");
    Script::newGame("pvp_lv2.esc");
    CHECK_EQ(Script::g_runDifficulty, Script::DIFFICULTY_NORMAL);
    CHECK(WaitForSetup(run, 10));
    CHECK(WaitForGround(run, 240, "pvp_lv2"));
    player = Player();
    CHECK(player != nullptr);
    if (player != nullptr) {
        // The arena is on screen from its first frame, so doLoop has already spawned its warrior; a marker still
        // waiting (a larger arena) is brought to the wizard as in the campaign's tests.
        ETHEntity pvpWarrior;
        ETHEntityArray arenaWarriors;
        GetEntityArray("warrior.ent", arenaWarriors);
        if (!arenaWarriors.empty()) pvpWarrior = arenaWarriors[0];
        if (pvpWarrior == nullptr) pvpWarrior = SpawnFromMarker(run, "warrior", player);
        CHECK(pvpWarrior != nullptr);
        if (pvpWarrior != nullptr) {
            CHECK_EQ(pvpWarrior->GetUIntData("pvpMode"), 1u);
            CHECK_EQ(pvpWarrior->GetIntData("hp"), 75);   // data.enml's: the arena's x5 is for the players
            CHECK_EQ(pvpWarrior->GetIntData("expGiven"), 75);
        }
    }
    Script::g_difficulty.setCurrent(Script::DIFFICULTY_NORMAL);
    CHECK_EQ(Script::g_runDifficulty, Script::DIFFICULTY_NORMAL);
}

// hs.enml keeps the original's list "hs" as Normal's and Hard's as a second entity, "hsHard".
void TestRecords(const std::filesystem::path& userRoot) {
    std::printf("-- E36: the best times, a list for each difficulty in the one file\n");
    const std::filesystem::path userHs = userRoot / "hs.enml";
    const auto reset = [&userHs] {
        std::error_code ignored;
        std::filesystem::remove(userHs, ignored);
    };
    // The text of a file's entity: "hs" up to "hsHard", "hsHard" to the end (the writer sorts the entities).
    const auto normalBlock = [](const string& bytes) { return bytes.substr(0, bytes.find("hsHard")); };
    const auto hardBlock = [](const string& bytes) {
        const std::size_t at = bytes.find("hsHard");
        return at == string::npos ? string() : bytes.substr(at);
    };
    const uint normal = Script::DIFFICULTY_NORMAL;
    const uint hard = Script::DIFFICULTY_HARD;
    const string fresh = "1    59:59\n2    59:59\n3    59:59\n4    59:59\n5    59:59\n";

    // A fresh state: nothing of the user's, the shipped file (only "hs") serves both lists as five 59:59.
    reset();
    CHECK(!std::filesystem::exists(userHs));
    CHECK(Script::getRecordTimeList() == fresh);
    CHECK(Script::getRecordTimeList(normal) == fresh);
    CHECK(Script::getRecordTimeList(hard) == fresh);
    CHECK_EQ(Script::getGetBestTime(), 3599000u);
    CHECK_EQ(Script::getGetBestTime(normal), Script::DEFAULT_RECORD_TIME);
    CHECK_EQ(Script::getGetBestTime(hard), Script::DEFAULT_RECORD_TIME);
    // The locked arenas (menu.esc's Templo Sagrado 720000 and Neblina 900000) stay locked: score <= best time.
    CHECK(720000u <= Script::getGetBestTime() && 900000u <= Script::getGetBestTime());
    // The panel's body: both lists, each under its name, Normal first; it fits the panel's 27 lines.
    CHECK(Script::recordsPanelText() == "Normal\n" + fresh + "\nDif\xED" "cil\n" + fresh);
    CHECK_EQ(LineCount(Script::recordsPanelText()), 14u);   // 13 lines (2 names, 10 times, a blank one) and the break that ends it
    CHECK(Script::difficultyName(normal) == "Normal");
    CHECK(Script::difficultyName(hard) == "Dif\xED" "cil");

    // A time that does not make the top five changes no time: the first write only creates the file, with BOTH entities.
    CHECK(!Script::addNewRecordTime(5000000u, normal));
    CHECK(std::filesystem::exists(userHs));
    {
        enmlFile file;
        CHECK_EQ(file.parseString(ReadUserHs(userRoot)), 0u);
        CHECK(file.exists("hs") && file.exists("hsHard"));
        for (uint t = 0; t < Script::MAX_SCORES; ++t) {
            uint v = 0;
            CHECK(file.getUint("hs", "hs" + std::to_string(t), v) && v == 3599000u);
            v = 0;
            CHECK(file.getUint("hsHard", "hs" + std::to_string(t), v) && v == 3599000u);
        }
    }
    CHECK(Script::getRecordTimeList(normal) == fresh && Script::getRecordTimeList(hard) == fresh);

    // A Hard time goes into Hard's list alone; a Normal time never wipes it, nor the reverse.
    reset();
    CHECK(Script::addNewRecordTime(850000u, hard));
    CHECK(Script::getRecordTimeList(hard) == "1    14:10\n2    59:59\n3    59:59\n4    59:59\n5    59:59\n");
    CHECK(Script::getRecordTimeList(normal) == fresh);
    CHECK(Script::getRecordTimeList() == fresh);
    CHECK_EQ(Script::getGetBestTime(hard), 850000u);
    CHECK_EQ(Script::getGetBestTime(normal), 3599000u);
    // The arena rule: the better of the two best times. Neblina (15:00) opens for a Hard finish of 14:10, Templo (12:00) not.
    CHECK_EQ(Script::getGetBestTime(), 850000u);
    CHECK(900000u > Script::getGetBestTime());
    CHECK(720000u <= Script::getGetBestTime());
    const string afterHard = ReadUserHs(userRoot);
    CHECK(!hardBlock(afterHard).empty());
    CHECK(Script::addNewRecordTime(700000u));   // the original's signature: a Normal time
    const string afterNormal = ReadUserHs(userRoot);
    CHECK(hardBlock(afterNormal) == hardBlock(afterHard));       // Hard's block, byte for byte
    CHECK(normalBlock(afterNormal) != normalBlock(afterHard));
    CHECK(Script::getRecordTimeList(normal) == "1    11:40\n2    59:59\n3    59:59\n4    59:59\n5    59:59\n");
    CHECK_EQ(Script::getGetBestTime(), 700000u);                 // Normal's is now the better: Templo opens too
    CHECK(720000u > Script::getGetBestTime());
    CHECK(Script::addNewRecordTime(950000u, hard));
    const string afterHard2 = ReadUserHs(userRoot);
    CHECK(normalBlock(afterHard2) == normalBlock(afterNormal));  // Normal's block, byte for byte
    CHECK(hardBlock(afterHard2) != hardBlock(afterNormal));
    CHECK(Script::getRecordTimeList(hard) == "1    14:10\n2    15:50\n3    59:59\n4    59:59\n5    59:59\n");

    // The top five, in order, and the sixth that does not make it: nothing changes, in either list.
    CHECK(Script::addNewRecordTime(100000u, hard));
    CHECK(Script::addNewRecordTime(200000u, hard));
    CHECK(Script::addNewRecordTime(300000u, hard));
    CHECK(Script::getRecordTimeList(hard) == "1    1:40\n2    3:20\n3    5:00\n4    14:10\n5    15:50\n");
    const string full = ReadUserHs(userRoot);
    CHECK(!Script::addNewRecordTime(960000u, hard));
    CHECK(ReadUserHs(userRoot) == full);
    CHECK(!Script::addNewRecordTime(5000000u, normal));
    CHECK(ReadUserHs(userRoot) == full);
    CHECK(Script::getRecordTimeList(normal) == "1    11:40\n2    59:59\n3    59:59\n4    59:59\n5    59:59\n");

    // A file written before E36 has only "hs": it is Normal's list as it was (keys it lacks read 59:59, not the key
    // before them and not 0), and Hard's is five 59:59.
    CHECK(SaveStringToFile(GetAbsolutePath("hs.enml"), "hs\n{\n\ths0 = 111000;\n\ths1 = 222000;\n}\n"));
    CHECK(Script::getRecordTimeList(normal) == "1    1:51\n2    3:42\n3    59:59\n4    59:59\n5    59:59\n");
    CHECK(Script::getRecordTimeList(hard) == fresh);
    CHECK_EQ(Script::getGetBestTime(), 111000u);
    CHECK(Script::addNewRecordTime(150000u));
    CHECK(Script::getRecordTimeList(normal) == "1    1:51\n2    2:30\n3    3:42\n4    59:59\n5    59:59\n");
    CHECK(Script::getRecordTimeList(hard) == fresh);
    CHECK(hardBlock(ReadUserHs(userRoot)) != string());          // the write added Hard's entity beside Normal's

    // A file with Hard's entity alone, and one that is no ENML at all: Normal 59:59 in the first, both in the second.
    CHECK(SaveStringToFile(GetAbsolutePath("hs.enml"), "hsHard\n{\n\ths0 = 5000;\n}\n"));
    CHECK(Script::getRecordTimeList(normal) == fresh);
    CHECK(Script::getRecordTimeList(hard) == "1    0:05\n2    59:59\n3    59:59\n4    59:59\n5    59:59\n");
    CHECK_EQ(Script::getGetBestTime(), 5000u);
    CHECK(SaveStringToFile(GetAbsolutePath("hs.enml"), "hs {\n\ths0 = \\x;\n}\n"));
    CHECK(Script::getRecordTimeList(normal) == fresh && Script::getRecordTimeList(hard) == fresh);
    CHECK_EQ(Script::getGetBestTime(), 3599000u);
    reset();
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
            TestDifficultyStats(run, player);   // E36
            TestHardRun(run);                   // E36: leaves the run in an arena
        }
        TestRecords(userRoot);                  // E36
        Trace(run, "end");
        PrintAborts(machine);
        CHECK_EQ(machine.ScriptAborts(), 0u);
    }

    std::filesystem::remove_all(userRoot, ec);
    return test::summary("test_pn_boot", 46);
}
