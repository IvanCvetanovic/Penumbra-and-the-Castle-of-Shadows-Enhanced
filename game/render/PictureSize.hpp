#pragma once

// The picture sharpness row of a phone's or tablet's options screen (E47), and what each of its values asks of the
// engine's DynamicResolution through RenderScaleControl (E50); and the scene the layer tells that controller it shows.
//
// E47 had one controller and three pins. On a slow GPU a pin was the wrong tool for "faster": Fast held 50% of the
// starting size while Automatic, free to fall to its floor, ran at 30%, so the value named for speed was the slower one.
// Since E50, Balanced and Fast are ceilings on the live controller: the larger sizes are taken away and everything else
// (its floors, its fall, its target of 58 fps) stays, so they buy frames as Automatic does, from a picture never larger
// than the ceiling, and they can go below it. They have no 30 fps tier: a player who picks them asks for frames, not for
// a steady 30 at a sharper picture. Sharp stays a pin, an explicit choice of the start size. The order, the ids
// written to settings.json and the cp1252 labels (which strings.json translates) are E47's, so a saved choice reads as
// before and no language needs a new word.
//
// THE SCENE (E50). The controller keeps a tier and a size per scene, so a heavy menu no longer decides a light level's
// size. Its key is the scene file as the game names it; its lines are the logical screen's height, GetScreenSize().y,
// which is the number of the art's lines on screen whatever the window's size: 768 for a menu or an unzoomed scene
// (PenumbraLayer::LogicalScreenFor) and 768 divided by the zoom for a zoomed campaign level (PhoneUi's ZoomedScreen:
// 439 at 175%). A level with fewer lines of art has less detail to lose, so its quality floor asks for no more lines.

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>

#include <glm/glm.hpp>

#include "core/RenderScaleControl.hpp"

namespace Penumbra::Render {

enum class PictureKind {
    Automatic,   // the game's own configuration: the two tiers (main.cpp)
    Pinned,      // held at `fraction` of the starting size, never resized
    Capped,      // never above `fraction` of the starting size, aiming at 58 fps only, without Automatic's 30 fps tier
};

struct PictureChoice {
    const char* id;      // the setting's word (Settings::pictureSize), lower case
    const char* label;   // the script's Portuguese, cp1252; strings.json and strings/<id>.json translate it
    PictureKind kind;
    float fraction;      // Pinned and Capped: the share of the size the controller starts at; 0 for Automatic
};

// The row's order; index 0 is the default, as the zoom's and the language's are.
inline constexpr PictureChoice kPictureChoices[] = {
    {"auto", "Autom\xE1tica", PictureKind::Automatic, 0.0f},
    {"sharp", "N\xEDtida", PictureKind::Pinned, 1.0f},
    {"balanced", "Equilibrada", PictureKind::Capped, 0.7f},
    {"fast", "R\xE1pida", PictureKind::Capped, 0.5f},
};
inline constexpr std::size_t kPictureChoiceCount = std::size(kPictureChoices);

// The choice an id names, in any case (Settings reads the setting in any case); nullptr for anything else.
const PictureChoice* PictureChoiceFor(std::string_view id);

// Asks the engine for the choice: RequestAutomatic, RequestFixed or RequestCapped. Latched; applied at the next frame.
void RequestPicture(const PictureChoice& choice, Supersonic::RenderScaleControl& control);

// What RenderScaleControl::NoteScene is told.
struct SceneNote {
    std::string key;           // the scene file, as Machine::GetSceneFileName names it
    std::uint32_t lines = 0;   // the art's lines on screen (0: unknown)
    bool operator==(const SceneNote&) const = default;
};
// The note for a scene file shown on a logical screen of `screenSize` (Machine::GetScreenSize).
SceneNote SceneNoteFor(std::string_view sceneFile, glm::vec2 screenSize);

} // namespace Penumbra::Render
