// playerInput.as, ported: every button the players have, and controlCharacter,
// which turns them into jumps, walking, combos, sword swings and spells.
// Original: extracted/app/playerInput.as (Penumbra, LGPL-3).
//
// Each button function reads the KEYBOARD first, and only for player 0, then
// that player's joystick. The first held key returns ITS state, so a held
// higher-priority key hides a new press of a lower one (holding Up makes a
// Ctrl press invisible to jump). That shadowing is the original's and is kept.

#include "script/Script.hpp"

namespace Penumbra::Script {

// playerInput.as:43. g_controls picks which pad belongs to which player; any
// other player index gets pad 2 (the third pad), which never happens: the
// scripts pass only 0 and 1.
uint getPlayerJoystick(const uint player) {
    switch (player) {
        case 0:
            return (g_controls.getCurrent() == 0 ? 1u : 0u);
        case 1:
            return (g_controls.getCurrent() == 0 ? 0u : 1u);
    };
    return 2;
}

// playerInput.as:55
KEY_STATE getLeftButtonStatus(const uint player) {
    InputState& input = GetInputHandle();

    if (player == 0) {
        if (input.KeyDown(K_LEFT)) {
            return input.GetKeyState(K_LEFT);
        }
    }

    if (input.GetJoystickStatus(getPlayerJoystick(player)) == JS_DETECTED) {
        return input.JoyButtonState(getPlayerJoystick(player), JK_LEFT);
    }

    return KS_UP;
}

// playerInput.as:75
KEY_STATE getRightButtonStatus(const uint player) {
    InputState& input = GetInputHandle();

    if (player == 0) {
        if (input.KeyDown(K_RIGHT)) {
            return input.GetKeyState(K_RIGHT);
        }
    }

    if (input.GetJoystickStatus(getPlayerJoystick(player)) == JS_DETECTED) {
        return input.JoyButtonState(getPlayerJoystick(player), JK_RIGHT);
    }

    return KS_UP;
}

// playerInput.as:95
KEY_STATE getUpButtonStatus(const uint player) {
    InputState& input = GetInputHandle();

    if (player == 0) {
        if (input.KeyDown(K_UP)) {
            return input.GetKeyState(K_UP);
        }
    }

    if (input.GetJoystickStatus(getPlayerJoystick(player)) == JS_DETECTED) {
        return input.JoyButtonState(getPlayerJoystick(player), JK_UP);
    }

    return KS_UP;
}

// playerInput.as:115
KEY_STATE getDownButtonStatus(const uint player) {
    InputState& input = GetInputHandle();

    if (player == 0) {
        if (input.KeyDown(K_DOWN)) {
            return input.GetKeyState(K_DOWN);
        }
    }

    if (input.GetJoystickStatus(getPlayerJoystick(player)) == JS_DETECTED) {
        return input.JoyButtonState(getPlayerJoystick(player), JK_DOWN);
    }

    return KS_UP;
}

// playerInput.as:135. Right wins over Left and Down over Up when both are
// held (the later assignment); the stick is ADDED unrounded, so any deflection
// past the dead zone walks at full speed (getInputDirection only reads signs).
vector2 getPlayerXYAxis(const uint player) {
    vector2 r(0.0f, 0.0f);
    InputState& input = GetInputHandle();

    if (player == 0) {
        if (input.KeyDown(K_LEFT))
            r.x = -1.0f;
        if (input.KeyDown(K_RIGHT))
            r.x = 1.0f;
        if (input.KeyDown(K_UP))
            r.y = -1.0f;
        if (input.KeyDown(K_DOWN))
            r.y = 1.0f;
    }

    if (input.GetJoystickStatus(getPlayerJoystick(player)) == JS_DETECTED) {
        r += input.GetJoystickXY(getPlayerJoystick(player));
    }
    return r;
}

// playerInput.as:159. True when player 1's pad (as g_controls assigns it) is
// present - the same pad getPlayerJoystick(1) returns.
bool hasASecondController() {
    InputState& input = GetInputHandle();
    return ((g_controls.getCurrent() == 1 && input.GetJoystickStatus(1) == JS_DETECTED)
            || (g_controls.getCurrent() == 0 && input.GetJoystickStatus(0) == JS_DETECTED));
}

// playerInput.as:166. No horizontal input reads as DOWN, which controlCharacter
// treats as "stand still".
uint getInputDirection(const uint player) {
    vector2 xy = getPlayerXYAxis(player);
    if (xy.x > 0)
        return RIGHT;
    else if (xy.x < 0)
        return LEFT;
    return DOWN;
}

// playerInput.as:176. Up doubles as jump; Up is checked first, so a held Up
// shadows a Ctrl press.
KEY_STATE getJumpButtonStatus(const uint player) {
    InputState& input = GetInputHandle();

    if (player == 0) {
        if (input.KeyDown(K_UP)) {
            return input.GetKeyState(K_UP);
        }

        if (input.KeyDown(K_CTRL)) {
            return input.GetKeyState(K_CTRL);
        }
    }

    if (input.GetJoystickStatus(getPlayerJoystick(player)) == JS_DETECTED) {
        return input.JoyButtonState(getPlayerJoystick(player), JK_03);
    }

    return KS_UP;
}

// playerInput.as:201 (sword)
KEY_STATE getAttack01ButtonStatus(const uint player) {
    InputState& input = GetInputHandle();

    if (player == 0) {
        if (input.KeyDown(K_S)) {
            return input.GetKeyState(K_S);
        }
    }

    if (input.GetJoystickStatus(getPlayerJoystick(player)) == JS_DETECTED) {
        return input.JoyButtonState(getPlayerJoystick(player), JK_04);
    }

    return KS_UP;
}

// playerInput.as:221 (fire ball)
KEY_STATE getAttack02ButtonStatus(const uint player) {
    InputState& input = GetInputHandle();

    if (player == 0) {
        if (input.KeyDown(K_D)) {
            return input.GetKeyState(K_D);
        }
    }

    if (input.GetJoystickStatus(getPlayerJoystick(player)) == JS_DETECTED) {
        return input.JoyButtonState(getPlayerJoystick(player), JK_02);
    }

    return KS_UP;
}

// playerInput.as:241 (light spell)
KEY_STATE getAttack03ButtonStatus(const uint player) {
    InputState& input = GetInputHandle();

    if (player == 0) {
        if (input.KeyDown(K_SPACE)) {
            return input.GetKeyState(K_SPACE);
        }
    }

    if (input.GetJoystickStatus(getPlayerJoystick(player)) == JS_DETECTED) {
        return input.JoyButtonState(getPlayerJoystick(player), JK_01);
    }

    return KS_UP;
}

// playerInput.as:261. Enter counts only while Alt is up, so Alt+Enter (the
// fullscreen toggle, menu.as:97) never confirms anything.
KEY_STATE getConfirmButtonStatus(const uint player) {
    InputState& input = GetInputHandle();

    if (player == 0) {
        if (input.KeyDown(K_RETURN) && input.GetKeyState(K_ALT) == KS_UP) {
            return input.GetKeyState(K_RETURN);
        }
        if (input.KeyDown(K_LMOUSE)) {
            return input.GetKeyState(K_LMOUSE);
        }
        if (input.KeyDown(K_RMOUSE)) {
            return input.GetKeyState(K_RMOUSE);
        }
    }

    if (input.GetJoystickStatus(getPlayerJoystick(player)) == JS_DETECTED) {
        return input.JoyButtonState(getPlayerJoystick(player), JK_10);
    }

    return KS_UP;
}

// playerInput.as:289
KEY_STATE getCancelButtonStatus(const uint player) {
    InputState& input = GetInputHandle();

    if (player == 0) {
        if (input.KeyDown(K_ESC)) {
            return input.GetKeyState(K_ESC);
        }
    }

    if (input.GetJoystickStatus(getPlayerJoystick(player)) == JS_DETECTED) {
        return input.JoyButtonState(getPlayerJoystick(player), JK_09);
    }

    return KS_UP;
}

// playerInput.as:309. Called by the player callbacks on every frame the player
// is not dead (controlCharacters.as:289-292, :330-335). The order - jump, walk,
// combos, sword, light, fire, then clearing g_castingLight - is load-bearing:
// see the notes inline.
void controlCharacter(ETHEntity thisEntity, const uint player) {
    // playerInput.as:311: fetched and never used in the original.
    [[maybe_unused]] InputState& input = GetInputHandle();
    const float speed = thisEntity->GetFloatData("speed");
    const bool touchingGround = (thisEntity->GetUIntData("touchingGround") != 0);

    // jump
    if (getJumpButtonStatus(player) == KS_HIT) {
        const int numJumps = thisEntity->GetIntData("jumps");

        // it can't jump in the air if it has no mana
        const bool mayJump = (!touchingGround && thisEntity->GetIntData("mp") < 4) ? false : true;
        if (numJumps < thisEntity->GetIntData("maxJumps") && mayJump) {
            if (mayJump && !touchingGround) {
                addToMp(thisEntity, -4);   // playerInput.as:326: an air jump costs 4 MP
            }

            setForceY(thisEntity, -thisEntity->GetFloatData("jumpForce"));
            thisEntity->AddIntData("jumps", numJumps + 1);
            if (!touchingGround) {
                g_camera.startEarthquake(2.5f);
                PlaySample("soundfx/jump0" + Str(player + 1) + ".ogg");   // playerInput.as:334: jump01/jump02
            }
        }
    }

    // playerInput.as:339-352: getInputDirection is evaluated twice, as in the
    // original (it only reads input, so the second call sees the same frame).
    if (getInputDirection(player) == LEFT) {
        thisEntity->AddUIntData("currentDir", LEFT);
        setForceX(thisEntity, -speed);
    } else if (getInputDirection(player) == RIGHT) {
        thisEntity->AddUIntData("currentDir", RIGHT);
        setForceX(thisEntity, speed);
    } else {
        setForceX(thisEntity, 0.0f);
    }

    bool didSwordCombo = false;
    bool didSpellCombo = false;

    // manage combos
    g_comboManager[player].updateInput(player);

    // sword combo
    if (g_comboManager[player].checkSequence(CMD_LEFT, CMD_LEFT, CMD_SWORD)
        || g_comboManager[player].checkSequence(CMD_RIGHT, CMD_RIGHT, CMD_SWORD)) {
        if (thisEntity->GetIntData("mp") >= 5) {
            // playerInput.as:366-371: the mana is spent and the screen shakes
            // even when swordAttack refuses the swing for its cooldown.
            addToMp(thisEntity, -5);
            swordAttack(thisEntity, "combo_sword.ent", "sword_beam.ent", true);
            if (thisEntity->GetFloatData("forceY") > 0)
                thisEntity->AddFloatData("forceY", 0.0f);   // playerInput.as:368: attacking stalls a fall
            didSwordCombo = true;
            g_camera.startEarthquake(7.0f);
        } else {
            // [It takes 5 mana to perform this combo]
            g_messages.addMessage("\xC9 necess\xE1rio ter 5 de mana para realizar este combo");
        }
    }

    // spell combo
    if (g_comboManager[player].checkSequence(CMD_DOWN, CMD_LEFT, CMD_SPELL)
        || g_comboManager[player].checkSequence(CMD_DOWN, CMD_RIGHT, CMD_SPELL)) {
        const int mana = 25;
        if (castSpell(thisEntity, "combo_fire_ball.ent", mana, vector3(0.0f, 0.0f, 0.0f), 225, 1.0f)) {
            PlaySample("soundfx/blast_attack.ogg");
            if (thisEntity->GetFloatData("forceY") > 0)
                thisEntity->AddFloatData("forceY", 0.0f);
            didSpellCombo = true;
        } else {
            // [It takes <mana> mana for this combo]
            g_messages.addMessage("\xC9 necess\xE1rio ter " + Str(mana) + " mana para este combo");
        }
    }

    // attack
    // playerInput.as:399: uint subtraction, wrapping as the original's did.
    if (getAttack01ButtonStatus(player) == KS_HIT
        && (GetTime() - thisEntity->GetUIntData("lastSwordAttack")) > thisEntity->GetUIntData("coolDown")
        && !didSwordCombo) {
        swordAttack(thisEntity, "sword" + Str(player) + ".ent", "sword_beam.ent", true);   // sword0.ent / sword1.ent
        if (thisEntity->GetFloatData("forceY") > 0)
            thisEntity->AddFloatData("forceY", 0.0f);
    }

    // light spell
    if (getAttack03ButtonStatus(player) == KS_HIT) {
        // g_castingLight[player] is true only if a light_spell callback ran
        // since this function last cleared it (playerInput.as:439, spells.as:85).
        if (!g_castingLight[player]) {
            const int mana = 50;
            if (castSpell(thisEntity, "light_spell.ent", mana, vector3(0.0f, -40.0f, 14.0f), 0, 1.0f))
                PlaySample("soundfx/light_spell.mp3");
            else
                // [It takes <mana> mana for this spell]
                g_messages.addMessage("\xC9 necess\xE1rio ter " + Str(mana) + " mana para esta magia");
        } else {
            // [This magic is already running]
            g_messages.addMessage("Esta m\xE1gica j\xE1 est\xE1 em execu\xE7\xE3o");
        }
    }

    // fire spell
    if (getAttack02ButtonStatus(player) == KS_HIT && !didSpellCombo) {
        const int mana = 10;
        if (castSpell(thisEntity, "fire_ball.ent", mana, vector3(0.0f, 0.0f, 0.0f), 30, 1.0f)) {
            PlaySample("soundfx/cast_fire_spell.ogg");
            if (thisEntity->GetFloatData("forceY") > 0)
                thisEntity->AddFloatData("forceY", 0.0f);
        } else {
            // [It takes <mana> mana for this spell]
            g_messages.addMessage("\xC9 necess\xE1rio ter " + Str(mana) + " mana para esta magia");
        }
    }
    g_castingLight[player] = false;   // playerInput.as:439: re-armed by ETHCallback_light_spell each frame
}

} // namespace Penumbra::Script
