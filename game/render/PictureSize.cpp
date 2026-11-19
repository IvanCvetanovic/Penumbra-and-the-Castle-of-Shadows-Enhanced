#include "render/PictureSize.hpp"

#include <cmath>

namespace Penumbra::Render {

namespace {

bool SameIdIgnoringCase(const std::string_view a, const std::string_view b) {
    if (a.size() != b.size()) return false;
    const auto lower = [](const char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}

} // namespace

const PictureChoice* PictureChoiceFor(const std::string_view id) {
    for (const PictureChoice& choice : kPictureChoices) {
        if (SameIdIgnoringCase(id, choice.id)) return &choice;
    }
    return nullptr;
}

void RequestPicture(const PictureChoice& choice, Supersonic::RenderScaleControl& control) {
    switch (choice.kind) {
    case PictureKind::Pinned:
        control.RequestFixed(choice.fraction);
        break;
    case PictureKind::Capped:
        control.RequestCapped(choice.fraction);
        break;
    case PictureKind::Automatic:
    default:   // a kind outside the enum: the controller's own choice, as an unknown setting is
        control.RequestAutomatic();
        break;
    }
}

SceneNote SceneNoteFor(const std::string_view sceneFile, const glm::vec2 screenSize) {
    SceneNote note;
    note.key = std::string(sceneFile);
    // Whole lines: a zoomed screen's height is already whole (ZoomedScreen rounds it); a height of no lines is unknown.
    note.lines = screenSize.y > 0.0f ? static_cast<std::uint32_t>(std::lround(screenSize.y)) : 0u;
    return note;
}

} // namespace Penumbra::Render
