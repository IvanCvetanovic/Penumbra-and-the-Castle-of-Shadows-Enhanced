#include "render/WideMenus.hpp"

#include <algorithm>
#include <map>
#include <tuple>
#include <vector>

namespace Penumbra::Render {

namespace {

// The tile rows each screen's backdrop continues, by entity name (the .esc
// files under extracted/app/scenes; Step 25 lists what each holds). Not the
// frames the rooms end in (videoModes.esc's half_wall01/02 at x 0-128 and
// 896-1024) nor its crystal: the backdrop is the floor and walls going on.
struct BackdropSpec {
    const char* scene;                 // the file name, as LoadScene's path ends
    std::vector<const char*> tiles;    // entity names whose rows are continued
};

const std::vector<BackdropSpec>& Specs() {
    static const std::vector<BackdropSpec> specs = {
        {"menu.esc", {"white_ground.ent"}},
        {"arena_select.esc", {"white_ground.ent"}},
        {"videoModes.esc", {"wall11.ent", "wall10.ent", "face_no_light.ent", "arch01.ent", "floor03.ent"}},
        {"gameover.esc", {}},
    };
    return specs;
}

// The file name at the end of a scene path ("scenes/menu.esc" -> "menu.esc").
std::string FileNameOf(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

const BackdropSpec* SpecFor(const std::string& sceneFile) {
    const std::string name = FileNameOf(sceneFile);
    for (const BackdropSpec& spec : Specs()) {
        if (name == spec.scene) return &spec;
    }
    return nullptr;
}

} // namespace

bool IsFixedLayoutScene(const std::string& sceneFile) { return SpecFor(sceneFile) != nullptr; }

bool WiderThan(glm::uvec2 window, glm::vec2 screen) {
    if (window.x == 0 || window.y == 0 || !(screen.x > 0.0f) || !(screen.y > 0.0f)) return false;
    // window.x / window.y > screen.x / screen.y, without a division's rounding:
    // 1024x768 against 1024x768 is exactly not wider.
    return static_cast<double>(window.x) * static_cast<double>(screen.y) >
           static_cast<double>(window.y) * static_cast<double>(screen.x);
}

Eth::SceneWidening WidenScene(const std::string& sceneFile, const Eth::SceneFile& file, float screenWidth,
                              float margin) {
    Eth::SceneWidening widening;
    const BackdropSpec* spec = SpecFor(sceneFile);
    if (spec == nullptr || !(margin > 0.0f)) return widening;
    widening.sideMargin = margin;

    // A row: one tile entity at one height and depth, in file order.
    using RowKey = std::tuple<std::string, float, float>;   // name, y, z
    std::map<RowKey, std::vector<const Eth::ScenePlacement*>> rows;
    std::vector<RowKey> order;   // rows in the order the file first lays them
    for (const Eth::ScenePlacement& placement : file.entities) {
        const bool tile = std::any_of(spec->tiles.begin(), spec->tiles.end(),
                                      [&placement](const char* name) { return placement.entityName == name; });
        if (!tile) continue;
        const RowKey key{placement.entityName, placement.position.y, placement.position.z};
        auto& row = rows[key];
        if (row.empty()) order.push_back(key);
        row.push_back(&placement);
    }

    for (const RowKey& key : order) {
        std::vector<const Eth::ScenePlacement*> row = rows[key];
        if (row.size() < 2) continue;   // no step to continue at
        std::stable_sort(row.begin(), row.end(), [](const Eth::ScenePlacement* a, const Eth::ScenePlacement* b) {
            return a->position.x < b->position.x;
        });
        // A regular row only: every tile one step after the last. A row with a
        // gap or a doubled tile is not a floor to go on with.
        const float step = row[1]->position.x - row[0]->position.x;
        if (!(step > 0.0f)) continue;
        bool regular = true;
        for (std::size_t i = 1; i < row.size(); ++i) {
            if (row[i]->position.x - row[i - 1]->position.x != step) regular = false;
        }
        if (!regular) continue;

        // Each copy is its row's outermost tile, moved a step at a time, while
        // its half-step reach still overlaps the margin.
        const auto copy = [&widening](const Eth::ScenePlacement& from, float x) {
            Eth::ScenePlacement tile = from;
            tile.position.x = x;
            widening.backdrop.push_back(std::move(tile));
        };
        const float half = step * 0.5f;
        for (float x = row.front()->position.x - step; x + half > -margin; x -= step) copy(*row.front(), x);
        for (float x = row.back()->position.x + step; x - half < screenWidth + margin; x += step) copy(*row.back(), x);
    }
    return widening;
}

} // namespace Penumbra::Render
