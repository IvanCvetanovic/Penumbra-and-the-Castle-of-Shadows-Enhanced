#pragma once

// ENHANCEMENT E1 for the screens laid out for 1024x768 (Step 25): in a window
// wider than 4:3 they fill it instead of being pillarboxed.
//
// The screens are menu.esc, arena_select.esc, videoModes.esc and gameover.esc:
// their buttons, panels and thumbnails sit at fixed pixels (menu.as,
// videoModes.as, gameover.as), so the scripts keep their 1024x768 screen
// (GetScreenSize) and everything they draw or hit-test stays where they put
// it, the screen centred in the window as the pillarbox centred it (the same
// scale and offset: a window wider than 4:3 already scaled it by its height).
// Only the side bars give way, to the world past the screen's left and right
// edges (Eth::SceneWidening, RenderSnapshot::sideMargin, View::openSides):
//
//   menu.esc, arena_select.esc   the floor of white_ground.ent tiles; the files
//                                lay it from x 0 to 2048, so the right side is
//                                the original's own art and the left side is
//                                the same tiles continued (the backdrop)
//   videoModes.esc               the room's columns of wall, face, arch and
//                                floor tiles, continued both ways (the file
//                                lays them from 0 to 1024 only)
//   gameover.esc                 black: the scene is the clouds and the fallen
//                                wizard on an unlit background
//
// A rectangle the scripts draw against the screen's left or right edge - the
// fades, the menu's panel - goes on to the window's (HudRenderer). The pointer
// maps as before; it now reaches the sides, where cursor.ent is drawn
// (InputMapper::WarpCursor, PointerOverBars). Tied to E1's switch (Widescreen,
// the options screen's "Tela larga (widescreen)"): off, the screens are the
// original's 4:3 with bars. Decided at each scene load, as the levels' width is.

#include <string>

#include <glm/glm.hpp>

#include "eth/Defs.hpp"
#include "eth/Machine.hpp"

namespace Penumbra::Render {

// The screen the fixed-layout scenes are laid out on.
inline constexpr glm::vec2 kFixedLayoutScreen{1024.0f, 768.0f};

// How far past the screen's left and right edges the wide menus show the
// world, logical pixels: a window up to 4:1 (1024 + 2 x 1024 across 768) is
// filled; past that, bars again (View::ShownMin/ShownMax).
inline constexpr float kWideMenuMargin = 1024.0f;

// The scenes laid out for the original's 1024x768 (the list the layer's
// E14 menu mode and its screen size read too).
bool IsFixedLayoutScene(const std::string& sceneFile);

// Whether a window of `window` pixels is wider than `screen`'s aspect: the
// sides show past the screen. Exactly 4:3 is not (1024x768 is drawn as it
// always was).
bool WiderThan(glm::uvec2 window, glm::vec2 screen);

// What E1 adds to a fixed-layout scene as it loads (MachineConfig::widenScene):
// the side margin, and the backdrop - each named row of tiles the file lays at
// a regular step continued outward from its outermost tile, at that step, as
// long as a tile still reaches into the margin (the screen `screenWidth` wide).
// Nothing for any other scene.
Eth::SceneWidening WidenScene(const std::string& sceneFile, const Eth::SceneFile& file,
                              float screenWidth = kFixedLayoutScreen.x, float margin = kWideMenuMargin);

} // namespace Penumbra::Render
