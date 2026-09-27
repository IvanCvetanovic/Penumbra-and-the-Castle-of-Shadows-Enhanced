// The Ethanon 0.7.12 runtime core (game/eth: Machine, Scene, Entity, the free
// functions, InputState, SampleBank) on hand-built definitions and registered
// lambdas - no file of the original is read. The semantics checked are
// docs/spec/30-ethanon-runtime.md §1-§2 and §4, each against the 0.7.12 source
// line it comes from.
//
// Two checks lean on other modules and say so: the temporary entity's
// auto-delete (ParticleManager, Particles.cpp) and the checkpoint round trip
// (WriteSceneFile/ReadSceneFile, Defs.cpp).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include "TestHarness.hpp"
#include "eth/Eth.hpp"
#include "eth/Scene.hpp"

using namespace Penumbra::Eth;

namespace {

constexpr float kPi = 3.14159265358979323846f;

MachineConfig SuiteConfig() {
    MachineConfig config;
    config.userRoot.clear();              // nothing is written unless a check asks
    config.screenSize = vector2(1024.0f, 768.0f);
    config.seed = 1234u;
    return config;
}

EntityDef MakeDef(const ENTITY_TYPE type, const bool isStatic) {
    EntityDef def;
    def.type = type;
    def.isStatic = isStatic;
    return def;
}

void Step(Machine& machine, const int frames = 1) {
    const InputFrame input;
    for (int i = 0; i < frames; ++i) machine.Frame(input);
}

// A machine in the empty scene ("") after its first frame.
void BootEmpty(Machine& machine, const string& preLoop = "", const string& loop = "") {
    machine.Boot([&] { LoadScene("", preLoop, loop); });
    Step(machine);
}

bool Same(const ETHEntity& handle, const std::shared_ptr<Entity>& entity) { return handle.Get() == entity.get(); }

bool Contains(const ETHEntityArray& array, const std::shared_ptr<Entity>& entity) {
    for (const ETHEntity& handle : array) {
        if (handle.Get() == entity.get()) return true;
    }
    return false;
}

// An AudioOut that remembers what it was told.
class Recorder final : public AudioOut {
public:
    bool Load(const string& absolutePath, const bool music) override {
        (void)absolutePath;
        (void)music;
        ++loads;
        return true;
    }
    VoiceId Play(const string& absolutePath, const bool loop, const float volume, const float pan) override {
        (void)absolutePath;
        ++plays;
        lastLoop = loop;
        lastVolume = volume;
        lastPan = pan;
        playing.insert(++next);
        return next;
    }
    void Stop(const VoiceId voice) override {
        playing.erase(voice);
        stopped.push_back(voice);
    }
    void Set(const VoiceId voice, const float volume, const float pan) override {
        (void)voice;
        ++sets;
        lastVolume = volume;
        lastPan = pan;
    }
    bool IsPlaying(const VoiceId voice) override { return playing.count(voice) != 0; }
    void UnloadAll() override { ++unloads; }

    std::set<VoiceId> playing;
    std::vector<VoiceId> stopped;
    VoiceId next = 0;
    int loads = 0;
    int plays = 0;
    int sets = 0;
    int unloads = 0;
    bool lastLoop = false;
    float lastVolume = -1.0f;
    float lastPan = 0.0f;
};

std::filesystem::path ScratchDir(const char* name) {
    std::error_code ec;
    const std::filesystem::path dir = std::filesystem::temp_directory_path(ec) / name;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

// --- Buckets --------------------------------------------------------------------

void TestBuckets() {
    Machine machine(SuiteConfig());
    Machine::Scope scope(machine);
    BootEmpty(machine);
    Scene* scene = machine.CurrentScene();
    CHECK(scene != nullptr);
    if (scene == nullptr) return;
    CHECK(GetSceneFileName() == "empty");
    CHECK(scene->BucketSize() == vector2(256.0f));

    // Horizontal entities go to the FRONT of their bucket, others to the back
    // (ETHScene.cpp:393-406).
    const auto h1 = scene->Add("h1", MakeDef(ET_HORIZONTAL, false), vector3(10, 10, 0), 0.0f);
    const auto v1 = scene->Add("v1", MakeDef(ET_VERTICAL, false), vector3(20, 20, 0), 0.0f);
    const auto h2 = scene->Add("h2", MakeDef(ET_HORIZONTAL, false), vector3(30, 30, 0), 0.0f);
    ETHEntityArray all;
    CHECK(GetEntitiesFromBucket(vector2(0, 0), all));
    CHECK_EQ(all.size(), 3u);
    if (all.size() == 3) {
        CHECK(Same(all[0], h2));
        CHECK(Same(all[1], h1));
        CHECK(Same(all[2], v1));
    }

    // It APPENDS, in list order.
    ETHEntityArray appended;
    appended.push_back(ETHEntity(v1));
    CHECK(GetEntitiesFromBucket(vector2(0, 0), appended));
    CHECK_EQ(appended.size(), 4u);
    if (appended.size() == 4) {
        CHECK(Same(appended[0], v1));
        CHECK(Same(appended[1], h2));
    }

    // A bucket nobody was ever in: false, nothing added. A non-integral key
    // could never have matched 0.7.12's hash map.
    ETHEntityArray none;
    CHECK(!GetEntitiesFromBucket(vector2(5, 5), none));
    CHECK_EQ(none.size(), 0u);
    CHECK(!GetEntitiesFromBucket(vector2(0.5f, 0.0f), none));

    // A move across a boundary relinks at once; a horizontal lands in front.
    const auto v2 = scene->Add("v2", MakeDef(ET_VERTICAL, false), vector3(300, 10, 0), 0.0f);
    ETHEntity(h1)->SetPositionXY(vector2(260, 10));
    CHECK(h1->GetCurrentBucket() == vector2(1, 0));
    ETHEntityArray moved;
    CHECK(GetEntitiesFromBucket(vector2(1, 0), moved));
    CHECK_EQ(moved.size(), 2u);
    if (moved.size() == 2) {
        CHECK(Same(moved[0], h1));
        CHECK(Same(moved[1], v2));
    }
    ETHEntityArray left;
    GetEntitiesFromBucket(vector2(0, 0), left);
    CHECK(!Contains(left, h1));
    // A move inside one bucket changes nothing of the order.
    ETHEntity(v1)->AddToPositionXY(vector2(1, 1));
    ETHEntityArray still;
    GetEntitiesFromBucket(vector2(0, 0), still);
    CHECK(still.size() == 2 && Same(still[0], h2) && Same(still[1], v1));

    // floor, not truncation: negative coordinates.
    const auto negative = scene->Add("n", MakeDef(ET_HORIZONTAL, false), vector3(-1.0f, -0.5f, 0), 0.0f);
    CHECK(negative->GetCurrentBucket() == vector2(-1, -1));
    CHECK(GetBucket(vector2(-1.0f, 255.9f)) == vector2(-1, 0));
    // The bucket it leaves stays, empty: false with an empty array...
    ETHEntity(negative)->SetPosition(vector3(600, 600, 0));
    ETHEntityArray emptied;
    CHECK(!GetEntitiesFromBucket(vector2(-1, -1), emptied));
    // ...and, as 0.7.12 tested the array's size, true when it already held something.
    emptied.push_back(ETHEntity(v1));
    CHECK(GetEntitiesFromBucket(vector2(-1, -1), emptied));

    // GetEntityArray appends exact name matches; false when none were added.
    ETHEntityArray named;
    CHECK(GetEntityArray("h2", named));
    CHECK(named.size() == 1 && Same(named[0], h2));
    CHECK(!GetEntityArray("H2", named));
    CHECK_EQ(named.size(), 1u);
    CHECK(SeekEntity("v2") == ETHEntity(v2));
    CHECK(SeekEntity(v2->GetID()) == ETHEntity(v2));
    CHECK(SeekEntity("nobody") == nullptr);
    CHECK_EQ(GetNumEntities(), 5u);

    // Visible buckets: inclusive at both ends - 5x4 at 1024x768 whether or not
    // the camera is aligned - plus a ring with border drawing, which every new
    // scene starts with ON (ETHScene.cpp:96).
    CHECK(IsDrawingBorderBuckets());
    CHECK_EQ(scene->VisibleBuckets(vector2(0, 0), vector2(1024, 768)).size(), std::size_t(42));
    SetBorderBucketsDrawing(false);
    CHECK_EQ(scene->VisibleBuckets(vector2(0, 0), vector2(1024, 768)).size(), std::size_t(20));
    CHECK_EQ(scene->VisibleBuckets(vector2(10.5f, 3.0f), vector2(1024, 768)).size(), std::size_t(20));
    const auto rows = scene->VisibleBuckets(vector2(0, 0), vector2(1024, 768));
    CHECK(rows.front() == Scene::BucketKey(0, 0));
    CHECK(rows[1] == Scene::BucketKey(1, 0));   // row-major
    CHECK(rows.back() == Scene::BucketKey(4, 3));

    // GetVisibleEntities: the camera at the time of the call.
    ETHEntityArray visible;
    GetVisibleEntities(visible);
    CHECK(Contains(visible, h2) && Contains(visible, negative));
    SetCameraPos(vector2(5000, 5000));
    ETHEntityArray far;
    GetVisibleEntities(far);
    CHECK_EQ(far.size(), 0u);
}

// --- Custom data, ids, soft delete -----------------------------------------------

void TestEntityData() {
    Machine machine(SuiteConfig());
    Machine::Scope scope(machine);
    BootEmpty(machine);
    Scene* scene = machine.CurrentScene();
    if (scene == nullptr) return;
    const ETHEntity e(scene->Add("thing.ent", MakeDef(ET_HORIZONTAL, false), vector3(40, 40, 0), 0.0f));

    // Typed values: a missing name or another type reads 0 / "".
    CHECK(e->AddIntData("a", 5));
    CHECK_EQ(e->GetIntData("a"), 5);
    CHECK_EQ(e->GetFloatData("a"), 0.0f);
    CHECK_EQ(e->GetUIntData("a"), 0u);
    CHECK(e->GetStringData("a").empty());
    CHECK(e->CheckCustomData("a") == DT_INT);
    CHECK(e->CheckCustomData("missing") == DT_NODATA);
    CHECK_EQ(e->GetIntData("missing"), 0);
    // Add* overwrites the type too.
    e->AddFloatData("a", 2.5f);
    CHECK_EQ(e->GetIntData("a"), 0);
    CHECK_EQ(e->GetFloatData("a"), 2.5f);
    CHECK(e->CheckCustomData("a") == DT_FLOAT);
    e->AddUIntData("u", 7u);
    CHECK_EQ(e->GetUIntData("u"), 7u);
    CHECK_EQ(e->GetIntData("u"), 0);
    e->AddStringData("s", "Po\xE7\xE3o");   // cp1252 bytes, as the scripts held them
    CHECK(e->GetStringData("s") == "Po\xE7\xE3o");
    CHECK_EQ(e->GetFloatData("s"), 0.0f);
    CHECK(e->EraseData("u"));
    CHECK(!e->EraseData("u"));
    CHECK(e->CheckCustomData("u") == DT_NODATA);
    CHECK(e->HasCustomData());

    // Frames: only f > cutX*cutY is refused (ETHEntity.cpp:131-145).
    EntityDef sheet = MakeDef(ET_HORIZONTAL, false);
    sheet.spriteCutX = 4;
    sheet.spriteCutY = 4;
    const ETHEntity s(scene->Add("sheet", sheet, vector3(0), 0.0f));
    CHECK(s->SetFrame(15u));
    CHECK(s->SetFrame(16u));
    CHECK_EQ(s->GetFrame(), 16u);
    CHECK(!s->SetFrame(17u));
    CHECK_EQ(s->GetFrame(), 0u);
    CHECK(s->SetFrame(1u, 2u));
    CHECK_EQ(s->GetFrame(), 9u);
    CHECK(!s->SetFrame(4u, 0u));
    CHECK_EQ(s->GetFrame(), 0u);

    // Sizes without a sprite (ETHRenderEntity.cpp:1004-1055).
    CHECK(e->GetSize() == vector2(32, 32));
    EntityDef box = MakeDef(ET_HORIZONTAL, false);
    box.collidable = true;
    box.collisionPos = vector3(0, 4, 0);
    box.collisionSize = vector3(14, 36, 10);
    const ETHEntity b(scene->Add("box", box, vector3(0), 0.0f));
    CHECK(b->GetSize() == vector2(14, 36));
    CHECK(b->GetCollisionBox().size == vector3(14, 36, 10));
    b->SetCollision(false);
    CHECK(b->GetCollisionBox().size == vector3(0));
    CHECK(b->GetSize() == vector2(32, 32));

    // Soft delete: unlinked and dead at once, the handle fully usable.
    const ETHEntity victim(scene->Add("victim.ent", MakeDef(ET_HORIZONTAL, false), vector3(50, 60, 1), 0.0f));
    victim->AddIntData("hp", 3);
    const int victimId = victim->GetID();
    const uint before = GetNumEntities();
    CHECK(DeleteEntity(victim) == nullptr);
    CHECK(!victim->IsAlive());
    CHECK(victim->GetPosition() == vector3(50, 60, 1));
    CHECK_EQ(victim->GetIntData("hp"), 3);
    victim->SetPositionXY(vector2(900, 900));   // writable, and relinks nothing
    CHECK(victim->GetPositionXY() == vector2(900, 900));
    CHECK_EQ(GetNumEntities(), before - 1);
    ETHEntityArray bucket;
    GetEntitiesFromBucket(vector2(0, 0), bucket);
    GetEntitiesFromBucket(vector2(3, 3), bucket);
    bool found = false;
    for (const ETHEntity& h : bucket) found = found || h == victim;
    CHECK(!found);
    CHECK(SeekEntity(victimId) == nullptr);
    // Not found any more: the handle comes back (ETHEngine.cpp:794-804).
    CHECK(DeleteEntity(victim) == victim);
    CHECK(DeleteEntity(ETHEntity()) == nullptr);
    // A null handle is AngelScript's null-pointer exception.
    bool threw = false;
    try {
        ETHEntity nothing;
        (void)nothing->GetID();
    } catch (const ScriptException&) {
        threw = true;
    }
    CHECK(threw);

    // A missing .ent: -1 and a null handle.
    ETHEntity out = e;
    CHECK_EQ(AddEntity("no_such_entity_file.ent", vector3(0), out), -1);
    CHECK(out == nullptr);
}

void TestIds() {
    Machine machine(SuiteConfig());
    Machine::Scope scope(machine);
    BootEmpty(machine);
    Scene* scene = machine.CurrentScene();
    if (scene == nullptr) return;
    CHECK_EQ(GetLastID(), 0);

    // A scene's own ids stay; the counter becomes max+1 (ETHScene.cpp:307).
    SceneFile file;
    file.properties.ambient = vector3(0.5f);
    for (const int id : {5, 12, 3}) {
        ScenePlacement placement;
        placement.id = id;
        placement.entityName = id == 12 ? "spawn" : "tile01.ent";
        placement.position = vector3(static_cast<float>(id) * 10.0f, 0, 0);
        placement.spriteFrame = 0;
        placement.def = MakeDef(ET_HORIZONTAL, true);
        file.entities.push_back(placement);
    }
    scene->Populate(file);
    CHECK(SeekEntity(12) != nullptr && SeekEntity(12)->GetEntityName() == "spawn");
    CHECK(SeekEntity(3) != nullptr);
    CHECK(GetAmbientLight() == vector3(0.5f));
    const auto first = scene->Add("a.ent", MakeDef(ET_HORIZONTAL, false), vector3(0), 0.0f);
    const auto second = scene->Add("b.ent", MakeDef(ET_HORIZONTAL, false), vector3(0), 0.0f);
    CHECK_EQ(first->GetID(), 13);
    CHECK_EQ(second->GetID(), 14);
    // GetLastID is the counter itself: the NEXT id (ETHScene.cpp:1680-1683).
    CHECK_EQ(GetLastID(), 15);
}

// --- The frame ----------------------------------------------------------------------

void TestFrameOrder() {
    Machine machine(SuiteConfig());
    Machine::Scope scope(machine);
    std::vector<string> log;
    std::vector<float> loopUps;      // UnitsPerSecond(60) seen by each loop call
    std::vector<float> callbackUps;  // ... and by each callback
    std::vector<uint> loopTimes;
    float preBUps = -1.0f;
    const auto tag = [&](const char* what) { return string(what) + "@" + std::to_string(machine.FrameIndex()); };

    machine.RegisterFunction("preA", [&] {
        log.push_back(tag("preA"));
        machine.CurrentScene()->Add("trigger.ent", MakeDef(ET_HORIZONTAL, false), vector3(100, 100, 0), 0.0f);
    });
    machine.RegisterFunction("loopA", [&] {
        log.push_back(tag("loopA"));
        loopUps.push_back(UnitsPerSecond(60.0f));
        loopTimes.push_back(GetTime());
    });
    machine.RegisterFunction("preB", [&] {
        log.push_back(tag("preB"));
        preBUps = UnitsPerSecond(60.0f);
    });
    machine.RegisterFunction("loopB", [&] {
        log.push_back(tag("loopB"));
        loopUps.push_back(UnitsPerSecond(60.0f));
    });
    machine.RegisterCallback("trigger", [&](ETHEntity self) {
        (void)self;
        log.push_back(tag("cb"));
        callbackUps.push_back(UnitsPerSecond(60.0f));
        // A callback's LoadScene: the OLD loop runs once more next frame, then
        // the load (docs/spec/30-ethanon-runtime.md §1).
        if (machine.FrameIndex() == 3) LoadScene("", "preB", "loopB");
    });
    machine.Boot([&] { LoadScene("", "preA", "loopA"); });
    CHECK_EQ(GetTime(), 0u);
    Step(machine, 5);

    const std::vector<string> expected = {"preA@1", "cb@1",   "loopA@2", "cb@2", "loopA@3",
                                          "cb@3",   "loopA@4", "preB@4", "loopB@5"};
    CHECK(log == expected);
    if (log != expected) {
        for (const string& line : log) std::printf("    got %s\n", line.c_str());
    }
    // 0 on the frame of a load: its callbacks and the next frame's loop see it;
    // the preLoop still sees the frame before (it ran before CalcLastFrame).
    CHECK(callbackUps.size() == 3 && callbackUps[0] == 0.0f && callbackUps[1] == 1.0f && callbackUps[2] == 1.0f);
    CHECK(loopUps.size() == 4 && loopUps[0] == 0.0f && loopUps[1] == 1.0f && loopUps[2] == 1.0f &&
          loopUps[3] == 0.0f);
    CHECK_EQ(preBUps, 1.0f);
    CHECK_EQ(GetFPSRate(), 60.0f);

    // GetTime: frame*1000/60 ms exactly, and a load resets nothing.
    CHECK(loopTimes.size() == 3 && loopTimes[0] == 33u && loopTimes[1] == 50u && loopTimes[2] == 66u);
    CHECK_EQ(GetTime(), 83u);
    Step(machine, 55);
    CHECK_EQ(machine.FrameIndex(), 60u);
    CHECK_EQ(GetTime(), 1000u);
    Step(machine);
    CHECK_EQ(GetTime(), 1016u);
    CHECK_EQ(machine.Snapshot().timeMs, 1016u);
    CHECK_EQ(machine.Snapshot().frameIndex, 61u);

    // The 1-argument LoadScene clears preLoop AND loop, and an empty loop name
    // leaves the running loop installed (ETHEngine.cpp:853-859, :903-911).
    log.clear();
    LoadScene("");
    Step(machine, 2);
    CHECK(log.size() == 2 && log[0] == "loopB@62" && log[1] == "loopB@63");
}

void TestHudQueue() {
    Machine machine(SuiteConfig());
    Machine::Scope scope(machine);
    machine.RegisterFunction("pre", [&] {
        machine.CurrentScene()->Add("hud.ent", MakeDef(ET_HORIZONTAL, false), vector3(0), 0.0f);
    });
    machine.RegisterFunction("loop", [&] {
        DrawRectangle(vector2(0), vector2(1), 0x10000000u + machine.FrameIndex(), 0, 0, 0);
        // Never loaded: nothing is drawn (ETHPrimitiveDrawer.cpp:158-168).
        DrawSprite("interface/not_loaded.png", vector2(0), 0xFFFFFFFFu);
    });
    machine.RegisterFunction("pre2", [&] {
        DrawRectangle(vector2(0), vector2(1), 0x30000000u + machine.FrameIndex(), 0, 0, 0);
    });
    machine.RegisterCallback("hud", [&](ETHEntity self) {
        (void)self;
        DrawText(vector2(0), "cb", "Arial", 30.0f, 0x20000000u + machine.FrameIndex());
        if (machine.FrameIndex() == 3) LoadScene("", "pre2", "loop");
    });
    machine.Boot([&] { LoadScene("", "pre", "loop"); });

    const auto colours = [&] {
        std::vector<uint> out;
        for (const HudCmd& cmd : machine.Snapshot().hud) out.push_back(cmd.color);
        return out;
    };
    Step(machine);   // frame 1: the load; no loop ran before it
    CHECK(colours().empty());
    Step(machine);   // frame 2: frame 1's callback draw, then this frame's loop draw
    CHECK(colours() == std::vector<uint>({0x20000001u, 0x10000002u}));
    CHECK(machine.Snapshot().hud.size() == 2 && machine.Snapshot().hud[0].kind == HudCmd::Kind::Text &&
          machine.Snapshot().hud[0].text == "cb");
    Step(machine);   // frame 3
    CHECK(colours() == std::vector<uint>({0x20000002u, 0x10000003u}));
    // Frame 4: LoadScene does not clear the queue - the callback's draw from
    // frame 3, the old loop's, then the new preLoop's.
    Step(machine);
    CHECK(colours() == std::vector<uint>({0x20000003u, 0x10000004u, 0x30000004u}));
    Step(machine);   // frame 5: the new scene has no callback entity
    CHECK(colours() == std::vector<uint>({0x10000005u}));
}

void TestStaticCallbacks() {
    Machine machine(SuiteConfig());
    Machine::Scope scope(machine);
    std::vector<string> ran;
    vector2 camera(0.0f);
    std::shared_ptr<Entity> hidden;
    machine.RegisterFunction("pre", [&] {
        Scene* scene = machine.CurrentScene();
        SetBorderBucketsDrawing(false);
        scene->Add("near.ent", MakeDef(ET_HORIZONTAL, true), vector3(100, 100, 0), 0.0f);
        scene->Add("edge.ent", MakeDef(ET_HORIZONTAL, true), vector3(1300, 100, 0), 0.0f);   // bucket (5,0)
        scene->Add("far.ent", MakeDef(ET_HORIZONTAL, true), vector3(5000, 5000, 0), 0.0f);
        hidden = scene->Add("hid.ent", MakeDef(ET_HORIZONTAL, true), vector3(200, 100, 0), 0.0f);
        hidden->Hide(true);
    });
    machine.RegisterFunction("loop", [&] { SetCameraPos(camera); });
    for (const char* name : {"near", "edge", "far", "hid"}) {
        const string n = name;
        machine.RegisterCallback(n, [&ran, n](ETHEntity self) {
            (void)self;
            ran.push_back(n);
        });
    }
    machine.Boot([&] { LoadScene("", "pre", "loop"); });
    Step(machine);
    // Only the static entities whose ORIGIN bucket was drawn, and not hidden.
    CHECK(ran == std::vector<string>({"near"}));
    ran.clear();
    SetBorderBucketsDrawing(true);   // the +1 ring reaches bucket (5,0)
    Step(machine);
    CHECK(ran.size() == 2 && std::find(ran.begin(), ran.end(), "edge") != ran.end());
    ran.clear();
    SetBorderBucketsDrawing(false);
    camera = vector2(4800, 4800);
    Step(machine);
    CHECK(ran == std::vector<string>({"far"}));
    ran.clear();
    camera = vector2(0.0f);
    hidden->Hide(false);
    Step(machine);
    CHECK(ran.size() == 2 && std::find(ran.begin(), ran.end(), "hid") != ran.end());
}

void TestScriptExceptions() {
    Machine machine(SuiteConfig());
    Machine::Scope scope(machine);
    int recorded = 0;
    int loops = 0;
    int afterThrow = 0;
    machine.RegisterFunction("pre", [&] {
        Scene* scene = machine.CurrentScene();
        scene->Add("thrower.ent", MakeDef(ET_HORIZONTAL, false), vector3(0), 0.0f);
        scene->Add("recorder.ent", MakeDef(ET_HORIZONTAL, false), vector3(0), 0.0f);
    });
    machine.RegisterFunction("loop", [&] {
        ++loops;
        array<int> values(2, 0);
        values[5] = 1;   // out of bounds: aborts the loop, as AngelScript did
        ++afterThrow;
    });
    machine.RegisterCallback("thrower", [&](ETHEntity self) {
        (void)self;
        ETHEntity nothing;
        (void)nothing->GetPosition();   // null handle: this callback ends here
        ++afterThrow;
    });
    machine.RegisterCallback("recorder", [&](ETHEntity self) {
        (void)self;
        ++recorded;
    });
    machine.Boot([&] { LoadScene("", "pre", "loop"); });
    Step(machine, 3);
    CHECK_EQ(recorded, 3);
    CHECK_EQ(loops, 2);
    CHECK_EQ(afterThrow, 0);
    CHECK_EQ(machine.ScriptAborts(), 5u);   // 3 callbacks + 2 loops
    CHECK(machine.Snapshot().frameIndex == 3u);
}

void TestCallbackList() {
    Machine machine(SuiteConfig());
    Machine::Scope scope(machine);
    std::vector<string> ran;
    ETHEntity doomed;
    machine.RegisterFunction("pre", [&] {
        Scene* scene = machine.CurrentScene();
        scene->Add("spawner.ent", MakeDef(ET_HORIZONTAL, false), vector3(0), 0.0f);
        doomed = ETHEntity(scene->Add("doomed.ent", MakeDef(ET_HORIZONTAL, false), vector3(0), 0.0f));
    });
    machine.RegisterCallback("spawner", [&](ETHEntity self) {
        (void)self;
        ran.push_back("spawner");
        if (machine.FrameIndex() == 2) {
            // Appended to the list being walked: runs this same pass. And a
            // deleted entity later in the list is skipped (ETHScene.cpp:977-985).
            machine.CurrentScene()->Add("child.ent", MakeDef(ET_HORIZONTAL, false), vector3(0), 0.0f);
            DeleteEntity(doomed);
        }
    });
    machine.RegisterCallback("doomed", [&](ETHEntity self) {
        (void)self;
        ran.push_back("doomed");
    });
    machine.RegisterCallback("child", [&](ETHEntity self) {
        (void)self;
        ran.push_back("child");
    });
    machine.Boot([&] { LoadScene("", "pre", ""); });
    Step(machine);
    CHECK(ran == std::vector<string>({"spawner", "doomed"}));
    ran.clear();
    Step(machine);
    CHECK(ran == std::vector<string>({"spawner", "child"}));
    ran.clear();
    Step(machine);
    CHECK(ran == std::vector<string>({"spawner", "child"}));
    CHECK(machine.CurrentScene()->DynamicOrTemporary().size() == 2);   // the dead one was dropped
}

// --- Snapshot -------------------------------------------------------------------------

void TestSnapshot() {
    MachineConfig config = SuiteConfig();
    config.screenSizeForScene = [](const string& file) {
        return file == "empty" ? vector2(1280.0f, 720.0f) : vector2(1024.0f, 768.0f);
    };
    Machine machine(config);
    Machine::Scope scope(machine);
    CHECK(GetScreenSize() == vector2(1024, 768));
    std::shared_ptr<Entity> back, front, same0, same1, layer, vertical;
    machine.RegisterFunction("pre", [&] {
        Scene* scene = machine.CurrentScene();
        SetBorderBucketsDrawing(false);
        layer = scene->Add("layer", [] {
            EntityDef d = MakeDef(ET_LAYERABLE, false);
            d.layerDepth = 1.0f;
            return d;
        }(), vector3(10, 10, 0), 0.0f);
        same1 = scene->Add("same1", MakeDef(ET_HORIZONTAL, false), vector3(300, 20, 0), 0.0f);   // bucket (1,0)
        same0 = scene->Add("same0", MakeDef(ET_HORIZONTAL, false), vector3(20, 20, 0), 0.0f);    // bucket (0,0)
        front = scene->Add("front", MakeDef(ET_HORIZONTAL, false), vector3(100.7f, 50.2f, 0), 0.0f);
        back = scene->Add("back", MakeDef(ET_HORIZONTAL, false), vector3(30, 30, -10), 0.0f);
        vertical = scene->Add("vert", MakeDef(ET_VERTICAL, false), vector3(300, 200, 0), 0.0f);
    });
    machine.Boot([&] { LoadScene("", "pre", ""); });
    Step(machine);
    CHECK(GetScreenSize() == vector2(1280, 720));   // chosen per scene at the load
    CHECK_EQ(GetScreenWidth(), 1280u);
    const RenderSnapshot& snap = machine.Snapshot();
    CHECK(snap.screenSize == vector2(1280, 720));
    CHECK(snap.sceneFile == "empty");
    CHECK_EQ(snap.sprites.size(), std::size_t(6));
    std::vector<string> order;
    for (const SpriteDraw& s : snap.sprites) order.push_back(s.entityName);
    // Lower z first; equal drawHash in visible-bucket order, then list order
    // (horizontals newest-first); the layerable at depth 1 and the vertical,
    // whose hash adds its screen y, last.
    const std::vector<string> expected = {"back", "front", "same0", "same1", "layer", "vert"};
    CHECK(order == expected);
    if (order != expected) {
        for (const SpriteDraw& s : snap.sprites) std::printf("    %s %f\n", s.entityName.c_str(), s.drawHash);
    }
    for (const SpriteDraw& s : snap.sprites) {
        if (s.entityName == "front") {
            // No sprite: 32x32, centred, the position floored (round-up is on).
            CHECK(s.size == vector2(32, 32));
            CHECK(s.origin == vector2(84, 34));
            CHECK(s.sprite.empty());
            CHECK(s.position == vector3(100.7f, 50.2f, 0));
        }
        if (s.entityName == "vert") CHECK(s.origin == vector2(284, 168));   // bottom-centre
        if (s.entityName == "layer") CHECK_EQ(s.depth, 1.0f);
    }
    // The depth range starts at [0, screen height] and grows by +-size per
    // entity: z=0 with a 32-high box reaches -32; z=-10 reaches -42.
    CHECK_EQ(machine.CurrentScene()->MinHeight(), -42.0f);
    CHECK_EQ(machine.CurrentScene()->MaxHeight(), 720.0f);
    SetPositionRoundUp(false);
    Step(machine);
    for (const SpriteDraw& s : machine.Snapshot().sprites) {
        if (s.entityName == "front") CHECK(std::fabs(s.origin.x - 84.7f) < 1e-3f);
    }
    // Camera: floored by SetCameraPos when round-up is on, never by AddToCameraPos.
    SetPositionRoundUp(true);
    SetCameraPos(vector2(10.6f, 20.2f));
    CHECK(GetCameraPos() == vector2(10, 20));
    AddToCameraPos(vector2(0.5f, 0.25f));
    CHECK(GetCameraPos() == vector2(10.5f, 20.25f));
    // A load puts the camera back at (0,0).
    LoadScene("");
    Step(machine);
    CHECK(GetCameraPos() == vector2(0, 0));
    // The minimum bucket size is 128; smaller keeps 256 (ETHScene.cpp:1848-1858).
    LoadScene("", "", "", vector2(64, 64));
    Step(machine);
    CHECK(machine.CurrentScene()->BucketSize() == vector2(256, 256));
    LoadScene("", "", "", vector2(1024, 256));
    Step(machine);
    CHECK(machine.CurrentScene()->BucketSize() == vector2(1024, 256));
    CHECK(IsDrawingBorderBuckets());
}

void TestCollide() {
    Machine machine(SuiteConfig());
    Machine::Scope scope(machine);
    BootEmpty(machine);
    Scene* scene = machine.CurrentScene();
    if (scene == nullptr) return;
    const auto boxed = [](const bool isStatic, const vector3 pos, const vector3 size) {
        EntityDef d = MakeDef(ET_HORIZONTAL, isStatic);
        d.collidable = true;
        d.collisionPos = pos;
        d.collisionSize = size;
        return d;
    };
    // The menu case (menu.as:246): a cursor box against a dynamic button.
    const ETHEntity cursor(scene->Add("cursor.ent", boxed(false, vector3(0, 0, -64), vector3(31, 31, 148)),
                                      vector3(100, 100, 0), 0.0f));
    const ETHEntity button(scene->Add("button", boxed(false, vector3(-18, 4, 0), vector3(369, 25, 25)),
                                      vector3(120, 100, 10), 0.0f));
    const ETHEntity wall(scene->Add("wall", boxed(true, vector3(0), vector3(40, 40, 40)), vector3(90, 90, 0),
                                    0.0f));
    ETHEntity hit;
    CHECK(CollideDynamic(cursor, hit));
    CHECK(hit == button);
    CHECK(CollideStatic(cursor, hit));
    CHECK(hit == wall);
    CHECK(Collide(cursor));
    // Touching counts (inclusive).
    const ETHEntity a(scene->Add("a", boxed(false, vector3(0), vector3(10, 10, 10)), vector3(1000, 1000, 0), 0.0f));
    const ETHEntity b(scene->Add("b", boxed(false, vector3(0), vector3(10, 10, 10)), vector3(1010, 1000, 0), 0.0f));
    CHECK(CollideDynamic(a, hit) && hit == b);
    b->SetPositionXY(vector2(1010.5f, 1000));
    CHECK(!CollideDynamic(a, hit));
    CHECK(hit == nullptr);   // a miss leaves the @&out null
    // Not collidable, or dead: false.
    b->SetPositionXY(vector2(1005, 1000));
    a->SetCollision(false);
    CHECK(!CollideDynamic(a));
    a->SetCollision(true);
    CHECK(CollideDynamic(a));
    DeleteEntity(a);
    CHECK(!CollideDynamic(a));
    bool threw = false;
    try {
        (void)Collide(ETHEntity());
    } catch (const ScriptException&) {
        threw = true;
    }
    CHECK(threw);
}

// --- Temporary entities (with Particles.cpp) ---------------------------------------------

void TestTemporaries() {
    Machine machine(SuiteConfig());
    Machine::Scope scope(machine);
    BootEmpty(machine);
    Scene* scene = machine.CurrentScene();
    if (scene == nullptr) return;
    EntityDef def = MakeDef(ET_HORIZONTAL, false);
    ParticleSystemDef system;
    system.bitmap = "fire.png";
    system.nParticles = 2;
    system.repeat = 1;
    system.lifeTime = 100.0f;
    system.allAtOnce = true;
    system.boundingSphere = 32.0f;
    system.size = 8.0f;
    system.maxSize = 16.0f;
    def.particles.push_back(system);
    const ETHEntity temp(scene->Add("fx.ent", def, vector3(200, 200, 0), 0.0f));
    // Temporary: no sprite, every system finite. SeekEntity never returns one.
    CHECK(temp->IsTemporary());
    CHECK(temp->HasParticleSystem());
    CHECK(temp->HasParticleSystem(0u) && !temp->HasParticleSystem(1u));
    CHECK(SeekEntity(temp->GetID()) == nullptr);
    CHECK(SeekEntity("fx.ent") == nullptr);
    CHECK_EQ(scene->DynamicOrTemporary().size(), std::size_t(1));
    // [Particles.cpp] The runtime deletes it once its particles are over.
    int frames = 0;
    while (temp->IsAlive() && frames < 240) {
        Step(machine);
        ++frames;
    }
    CHECK(!temp->IsAlive());
    CHECK(frames > 1);
    CHECK_EQ(GetNumEntities(), 0u);
    CHECK_EQ(scene->DynamicOrTemporary().size(), std::size_t(0));
}

// --- Files, save and checkpoint ---------------------------------------------------------------

void TestFiles() {
    const std::filesystem::path user = ScratchDir("penumbra_test_pn_runtime");
    MachineConfig config = SuiteConfig();
    config.userRoot = user.generic_string();
    Machine machine(config);
    Machine::Scope scope(machine);

    const string game = config.gameRoot;
    CHECK(GetAbsolutePath("hs.enml") == game + "/hs.enml");
    CHECK(GetProgramPath() == game);
    CHECK(machine.WritePath("hs.enml") == config.userRoot + "/hs.enml");
    CHECK(machine.WritePath(GetAbsolutePath("scenes/checkpoint.esc")) == config.userRoot + "/scenes/checkpoint.esc");
    CHECK(std::filesystem::is_directory(user / "scenes"));
    CHECK(machine.WritePath((user / "elsewhere.txt").generic_string()).empty());
    CHECK(machine.ReadPath("hs.enml") == game + "/hs.enml");   // no user copy yet
    CHECK(machine.IsGamePath("data.enml") && machine.IsGamePath(GetAbsolutePath("data.enml")));

    // GetStringFromFile reads as a text-mode stream did: CRLF -> LF, NULs gone.
    const string loose = (user / "loose.txt").generic_string();
    {
        std::ofstream out(loose, std::ios::binary);
        const char bytes[] = {'a', '\r', '\n', 'b', '\0', 'c'};
        out.write(bytes, sizeof(bytes));
    }
    CHECK(GetStringFromFile(loose) == "a\nbc");
    CHECK(GetStringFromFile((user / "missing.txt").generic_string()).empty());
    // SaveStringToFile writes LF as CRLF; a game path lands in the user root.
    CHECK(SaveStringToFile(GetAbsolutePath("hs.enml"), "x\ny"));
    CHECK(machine.ReadPath("hs.enml") == config.userRoot + "/hs.enml");
    {
        std::ifstream in(user / "hs.enml", std::ios::binary);
        const string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(bytes == "x\r\ny");
    }
    CHECK(GetStringFromFile(GetAbsolutePath("hs.enml")) == "x\ny");

    // [Defs.cpp] SaveScene -> the user copy -> LoadScene("scenes/checkpoint.esc").
    std::shared_ptr<Entity> saved;
    machine.RegisterFunction("pre", [&] {
        EntityDef def = MakeDef(ET_HORIZONTAL, true);
        def.collidable = true;
        def.collisionSize = vector3(10, 10, 10);
        saved = machine.CurrentScene()->Add("checkpoint.ent", def, vector3(64, 32, 0), 0.0f, 41);
        machine.CurrentScene()->Add("other.ent", MakeDef(ET_VERTICAL, false), vector3(512, 32, 0), 0.0f);
    });
    machine.RegisterFunction("restored", [] {});
    machine.Boot([&] { LoadScene("", "pre", ""); });
    Step(machine);
    SetAmbientLight(vector3(0.25f, 0.5f, 0.75f));
    saved->AddIntData("lives", 13);
    saved->AddStringData("name", "level2.esc");
    saved->SetCollision(false);
    saved->SetFrame(1u);
    CHECK(SaveScene("scenes/checkpoint.esc"));
    CHECK(std::filesystem::exists(user / "scenes" / "checkpoint.esc"));
    LoadScene("scenes/checkpoint.esc", "restored", "");
    Step(machine);
    CHECK(GetSceneFileName() == "scenes/checkpoint.esc");
    CHECK_EQ(GetNumEntities(), 2u);
    const ETHEntity restored = SeekEntity(41);
    CHECK(restored != nullptr);
    if (restored != nullptr) {
        CHECK(restored->GetEntityName() == "checkpoint.ent");
        CHECK(restored->GetPosition() == vector3(64, 32, 0));
        CHECK_EQ(restored->GetIntData("lives"), 13);
        CHECK(restored->GetStringData("name") == "level2.esc");
        CHECK(!restored->Collidable());
        CHECK_EQ(restored->GetFrame(), 1u);
    }
    CHECK(GetAmbientLight() == vector3(0.25f, 0.5f, 0.75f));
    // Ids kept (41, and 0 for the one numbered by the counter - a forced id
    // never moves it, only a file's do); the counter is max+1 again.
    CHECK(SeekEntity(0) != nullptr);
    CHECK_EQ(GetLastID(), 42);

    // A scene with nothing in it is not saved.
    LoadScene("");
    Step(machine);
    CHECK(!SaveScene("scenes/empty.esc"));

    std::error_code ec;
    std::filesystem::remove_all(user, ec);
}

// --- Audio ---------------------------------------------------------------------------------------

void TestSamples() {
    Machine machine(SuiteConfig());
    Machine::Scope scope(machine);
    Recorder out;
    machine.Samples().SetOutput(&out);
    BootEmpty(machine);

    CHECK(!PlaySample("soundfx/fase.mp3"));        // never auto-loads
    CHECK(LoadMusic("soundfx/fase.mp3"));
    CHECK(LoadMusic("soundfx/fase.mp3"));          // a no-op the second time
    CHECK_EQ(out.loads, 1);
    CHECK(SampleExists("fase.mp3"));               // keyed by basename
    CHECK(SampleExists("other/dir/fase.mp3"));
    CHECK(!SampleExists("soundfx/menu.mp3"));
    CHECK(PlaySample("soundfx/fase.mp3"));
    const VoiceId first = out.next;
    CHECK(IsSamplePlaying("soundfx/fase.mp3"));
    // One voice per file: playing again restarts, never overlaps.
    CHECK(PlaySample("soundfx/fase.mp3"));
    CHECK(std::find(out.stopped.begin(), out.stopped.end(), first) != out.stopped.end());
    CHECK_EQ(out.playing.size(), std::size_t(1));
    // Volume: linear, clamped, persistent.
    CHECK(SetSampleVolume("soundfx/fase.mp3", 2.0f));
    CHECK_EQ(out.lastVolume, 1.0f);
    CHECK(SetSampleVolume("soundfx/fase.mp3", 0.25f));
    CHECK_EQ(out.lastVolume, 0.25f);
    CHECK(LoopSample("soundfx/fase.mp3", true));
    CHECK(out.lastLoop);
    CHECK_EQ(out.lastVolume, 0.25f);
    CHECK(IsSamplePlaying("soundfx/fase.mp3"));
    CHECK(StopSample("soundfx/fase.mp3"));
    CHECK(!IsSamplePlaying("soundfx/fase.mp3"));
    CHECK(PlaySample("soundfx/fase.mp3"));
    CHECK(out.lastLoop);   // the loop flag persists
    // Every load stops and forgets every sample.
    LoadScene("");
    Step(machine);
    CHECK(!SampleExists("fase.mp3"));
    CHECK(out.playing.empty());
    CHECK(out.unloads >= 1);

    // A particle system's sound effect is the same sample object.
    machine.Samples().StartEffect("sword01.mp3", 0.7f);
    CHECK(SampleExists("soundfx/sword01.mp3"));
    CHECK_EQ(out.lastVolume, 0.7f);
    machine.Samples().ApplyEffect("sword01.mp3", false, false, 0.5f, -0.5f);
    CHECK_EQ(out.lastVolume, 0.5f);
    CHECK_EQ(out.lastPan, -0.5f);
    machine.Samples().ApplyEffect("sword01.mp3", true, false, 0.5f, -0.5f);
    CHECK(!IsSamplePlaying("soundfx/sword01.mp3"));
    // The loop rule: not playing -> Play() from the start with the loop flag.
    const int plays = out.plays;
    machine.Samples().ApplyEffect("sword01.mp3", false, true, 0.4f, 0.0f);
    CHECK_EQ(out.plays, plays + 1);
    CHECK(out.lastLoop);
    CHECK(IsSamplePlaying("soundfx/sword01.mp3"));
    machine.Samples().SetOutput(nullptr);
}

// --- Input -----------------------------------------------------------------------------------------

void TestInput() {
    InputState input;
    InputFrame frame;
    frame.keys[K_A] = true;
    input.Update(frame);
    CHECK(input.GetKeyState(K_A) == KS_HIT);
    CHECK(input.KeyDown(K_A));
    input.Update(frame);
    CHECK(input.GetKeyState(K_A) == KS_DOWN);
    frame.keys[K_A] = false;
    input.Update(frame);
    CHECK(input.GetKeyState(K_A) == KS_RELEASE);
    CHECK(!input.KeyDown(K_A));
    input.Update(frame);
    CHECK(input.GetKeyState(K_A) == KS_UP);
    // Focus lost: every key reads UP at once, with no RELEASE.
    frame.keys[K_ENTER] = true;
    input.Update(frame);
    frame.hasFocus = false;
    input.Update(frame);
    CHECK(input.GetKeyState(K_RETURN) == KS_UP);
    frame.hasFocus = true;
    frame.keys[K_ENTER] = false;

    // Pads: dead zone 0.01, arrows at |axis| >= 0.8 with y DOWN.
    frame.pads[0].connected = true;
    frame.pads[0].xy = vector2(0.005f, -0.9f);
    frame.pads[0].buttons[0] = true;
    input.Update(frame);
    CHECK(input.GetJoystickStatus(0) == JS_DETECTED);
    CHECK(input.GetJoystickStatus(1) == JS_NOTDETECTED);
    CHECK(input.GetJoystickStatus(7) == JS_INVALID);
    CHECK(input.DetectJoysticks());
    CHECK(input.GetJoystickXY(0) == vector2(0.0f, -0.9f));
    CHECK(input.JoyButtonState(0, JK_UP) == KS_HIT);
    CHECK(input.JoyButtonState(0, JK_DOWN) == KS_UP);
    CHECK(input.JoyButtonState(0, JK_01) == KS_HIT);
    CHECK(input.JoyButtonDown(0, JK_01));
    CHECK(input.JoyButtonState(3, JK_01) == KS_UP);
    frame.pads[0].xy = vector2(0.8f, 0.79f);
    input.Update(frame);
    CHECK(input.JoyButtonState(0, JK_UP) == KS_RELEASE);
    CHECK(input.JoyButtonState(0, JK_RIGHT) == KS_HIT);
    CHECK(input.JoyButtonState(0, JK_DOWN) == KS_UP);
    CHECK(input.JoyButtonState(0, JK_01) == KS_DOWN);

    // The cursor: SetCursorPos is a request the layer takes.
    vector2 request(0.0f);
    CHECK(!input.TakeCursorRequest(request));
    input.SetCursorPos(vector2(640, 360));
    CHECK(input.TakeCursorRequest(request));
    CHECK(request == vector2(640, 360));
    CHECK(!input.TakeCursorRequest(request));
    frame.typed = "ab\r";
    input.Update(frame);
    CHECK(input.GetLastCharInput() == "b");
    frame.typed = "\t";
    input.Update(frame);
    CHECK(input.GetLastCharInput() == "    ");
    frame.typed.clear();
    input.Update(frame);
    CHECK(input.GetLastCharInput().empty());
}

// --- Numbers -----------------------------------------------------------------------------------------

void TestNumbers() {
    Machine machine(SuiteConfig());
    Machine::Scope scope(machine);
    // atan2(x, y): measured from +Y towards +X, in [0, 2pi).
    CHECK_NEAR(GetAngle(vector2(0, 1)), 0.0f);
    CHECK_NEAR(GetAngle(vector2(1, 0)), kPi / 2.0f);
    CHECK_NEAR(GetAngle(vector2(0, -1)), kPi);
    CHECK_NEAR(GetAngle(vector2(-1, 0)), 3.0f * kPi / 2.0f);
    CHECK_NEAR(radianToDegree(kPi), 180.0f);
    CHECK_NEAR(degreeToRadian(90.0f), kPi / 2.0f);
    const vector2 unit = normalize(vector2(3, 4));
    CHECK_NEAR(unit.x, 0.6f);
    CHECK_NEAR(unit.y, 0.8f);
    const vector2 zero = normalize(vector2(0, 0));
    CHECK(std::isnan(zero.x) && std::isnan(zero.y));
    CHECK(std::isnan(normalize(vector3(0)).z));
    // Inclusive ranges.
    bool sawZero = false;
    bool sawMax = false;
    bool inRange = true;
    for (int i = 0; i < 2000; ++i) {
        const int r = rand(3);
        sawZero = sawZero || r == 0;
        sawMax = sawMax || r == 3;
        inRange = inRange && r >= 0 && r <= 3;
        const float f = randF(2.0f, 4.0f);
        inRange = inRange && f >= 2.0f && f <= 4.0f;
    }
    CHECK(sawZero && sawMax && inRange);
    const int r = rand(5, 7);
    CHECK(r >= 5 && r <= 7);
}

} // namespace

int main() {
    TestBuckets();
    TestEntityData();
    TestIds();
    TestFrameOrder();
    TestHudQueue();
    TestStaticCallbacks();
    TestScriptExceptions();
    TestCallbackList();
    TestSnapshot();
    TestCollide();
    TestTemporaries();
    TestFiles();
    TestSamples();
    TestInput();
    TestNumbers();
    return test::summary("test_pn_runtime", 150);
}
