// What AngelScript's module lookup did for the original: 0.7.12 bound an entity
// to "ETHCallback_" + its file name without the extension when the entity was
// created (docs/spec/30-ethanon-runtime.md), and LoadScene found its preLoop
// and loop functions by name. The Machine resolves both through these tables,
// so every callback the scripts define and every function a LoadScene names
// must be registered here under the original's exact name.

#include "script/Script.hpp"

namespace Penumbra::Script {

void RegisterAll(Machine& machine)
{
    // --- ETHCallback_* (36), keyed by the name after the prefix ---------------

    // main.as
    machine.RegisterCallback("help", ETHCallback_help);                       // main.as:147
    machine.RegisterCallback("story", ETHCallback_story);                     // main.as:163
    machine.RegisterCallback("checkpoint", ETHCallback_checkpoint);           // main.as:170
    machine.RegisterCallback("next_level", ETHCallback_next_level);           // main.as:194

    // controlCharacters.as
    machine.RegisterCallback("bruxo", ETHCallback_bruxo);                     // controlCharacters.as:270
    machine.RegisterCallback("princess", ETHCallback_princess);               // controlCharacters.as:304
    machine.RegisterCallback("warrior", ETHCallback_warrior);                 // controlCharacters.as:471
    machine.RegisterCallback("minion", ETHCallback_minion);                   // controlCharacters.as:495
    machine.RegisterCallback("knight", ETHCallback_knight);                   // controlCharacters.as:519
    machine.RegisterCallback("impy", ETHCallback_impy);                       // controlCharacters.as:543
    machine.RegisterCallback("master_knight", ETHCallback_master_knight);     // controlCharacters.as:567
    machine.RegisterCallback("paladin", ETHCallback_paladin);                 // controlCharacters.as:591
    machine.RegisterCallback("king", ETHCallback_king);                       // controlCharacters.as:645

    // swords.as
    machine.RegisterCallback("enemy_sword", ETHCallback_enemy_sword);         // swords.as:109
    machine.RegisterCallback("dark_sword", ETHCallback_dark_sword);           // swords.as:114
    machine.RegisterCallback("sword0", ETHCallback_sword0);                   // swords.as:120
    machine.RegisterCallback("sword1", ETHCallback_sword1);                   // swords.as:124
    machine.RegisterCallback("combo_sword", ETHCallback_combo_sword);         // swords.as:128
    machine.RegisterCallback("paladin_sword", ETHCallback_paladin_sword);     // swords.as:133

    // spells.as
    machine.RegisterCallback("light_spell", ETHCallback_light_spell);         // spells.as:83
    machine.RegisterCallback("fire_ball", ETHCallback_fire_ball);             // spells.as:113
    machine.RegisterCallback("combo_fire_ball", ETHCallback_combo_fire_ball); // spells.as:118

    // potions.as
    machine.RegisterCallback("potion", ETHCallback_potion);                   // potions.as:63
    machine.RegisterCallback("potion_small", ETHCallback_potion_small);       // potions.as:68
    machine.RegisterCallback("potion_large", ETHCallback_potion_large);       // potions.as:73

    // lavaShooter.as
    machine.RegisterCallback("shooter", ETHCallback_shooter);                 // lavaShooter.as:43
    machine.RegisterCallback("fire_shoot", ETHCallback_fire_shoot);           // lavaShooter.as:64

    // environment.as
    machine.RegisterCallback("clouds", ETHCallback_clouds);                   // environment.as:43
    machine.RegisterCallback("fog", ETHCallback_fog);                         // environment.as:115
    machine.RegisterCallback("dawn", ETHCallback_dawn);                       // environment.as:145

    // events.as
    machine.RegisterCallback("event01", ETHCallback_event01);                 // events.as:43
    machine.RegisterCallback("falling_bridge", ETHCallback_falling_bridge);   // events.as:67

    // menu.as
    machine.RegisterCallback("cursor", ETHCallback_cursor);                   // menu.as:232
    machine.RegisterCallback("thumbnail", ETHCallback_thumbnail);             // menu.as:362

    // videoModes.as
    machine.RegisterCallback("picker", ETHCallback_picker);                   // videoModes.as:58

    // setupScene.as
    machine.RegisterCallback("play", ETHCallback_play);                       // setupScene.as:43

    // --- LoadScene preLoop / loop targets (9) -----------------------------------
    // Named by main.as:116,120,126,224, controlCharacters.as:438-449,
    // menu.as:309,387,398 and setupScene.as:245.
    machine.RegisterFunction("setupScene", setupScene);                       // setupScene.as:114
    machine.RegisterFunction("levelLoop", levelLoop);                         // setupScene.as:226
    machine.RegisterFunction("pvpLoop", pvpLoop);                             // setupScene.as:231
    machine.RegisterFunction("menuPreLoop", menuPreLoop);                     // menu.as:50
    machine.RegisterFunction("menuLoop", menuLoop);                           // menu.as:92
    machine.RegisterFunction("gameOverPreLoop", gameOverPreLoop);             // gameover.as:45
    machine.RegisterFunction("gameOverLoop", gameOverLoop);                   // gameover.as:62
    machine.RegisterFunction("screenModesPreLoop", screenModesPreLoop);       // videoModes.as:52
    machine.RegisterFunction("screenModesLoop", screenModesLoop);             // videoModes.as:85
}

} // namespace Penumbra::Script
