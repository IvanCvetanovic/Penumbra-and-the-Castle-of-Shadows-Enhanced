// combo.as, ported: the per-player combo buffer (g_comboManager, main.as:60).
// Original: extracted/app/combo.as (Penumbra, LGPL-3).
//
// A 5-slot buffer that records at most ONE command per frame (the first HIT in
// the order Left, Right, Up, Down, Sword, Spell) and empties itself after
// BUTTON_STRIDE ms with no new command. The constants are in Script.hpp
// (combo.as:43-52).

#include "script/Script.hpp"

namespace Penumbra::Script {

// combo.as:56. lastInput and index start at 0 through their initializers
// (combo.as:59-60).
//
// The original's uint[] resize left the five slots as whatever the heap held
// (0.7.12's addons/scriptarray.cpp: Resize constructs only object subtypes);
// here they start as CMD_NONE. Either way the first idle frame zero()es them.
Combo::Combo() {
    m_keys.resize(MAX_COMBO_KEYS);
}

// combo.as:63. Called once per frame from controlCharacter (playerInput.as:358).
void Combo::updateInput(const uint player) {
    // combo.as:65-66: past five keys, each new one overwrites the last slot.
    if (m_index >= MAX_COMBO_KEYS)
        m_index = MAX_COMBO_KEYS - 1;

    if (getLeftButtonStatus(player) == KS_HIT) {
        m_keys[m_index] = CMD_LEFT;
        m_lastInput = GetTime();
        m_index++;
        return;
    }

    if (getRightButtonStatus(player) == KS_HIT) {
        m_keys[m_index] = CMD_RIGHT;
        m_lastInput = GetTime();
        m_index++;
        return;
    }

    if (getUpButtonStatus(player) == KS_HIT) {
        m_keys[m_index] = CMD_UP;
        m_lastInput = GetTime();
        m_index++;
        return;
    }

    if (getDownButtonStatus(player) == KS_HIT) {
        m_keys[m_index] = CMD_DOWN;
        m_lastInput = GetTime();
        m_index++;
        return;
    }

    if (getAttack01ButtonStatus(player) == KS_HIT) {
        m_keys[m_index] = CMD_SWORD;
        m_lastInput = GetTime();
        m_index++;
        return;
    }

    if (getAttack02ButtonStatus(player) == KS_HIT) {
        m_keys[m_index] = CMD_SPELL;
        m_lastInput = GetTime();
        m_index++;
        return;
    }

    // combo.as:116: uint subtraction, wrapping as the original's did. The
    // `index == MAX_COMBO_KEYS` half can never be true here (the clamp above
    // ran first and every path that increments returns), kept as written.
    if (GetTime() - m_lastInput > BUTTON_STRIDE || m_index == MAX_COMBO_KEYS) {
        zero();
    }
}

// combo.as:122
void Combo::zero() {
    m_index = 0;
    for (uint t = 0; t < MAX_COMBO_KEYS; t++) {
        m_keys[t] = CMD_NONE;
    }
}

// combo.as:131. Matches only the FIRST three commands since the last reset.
bool Combo::checkSequence(const uint a, const uint b, const uint c) {
    if (m_keys[0] == a && m_keys[1] == b && m_keys[2] == c) {
        zero();
        return true;
    }
    return false;
}

// combo.as:141. Never called by the scripts.
bool Combo::checkSequence(const uint a, const uint b, const uint c, const uint d, const uint e) {
    if (m_keys[0] == a && m_keys[1] == b && m_keys[2] == c
        && m_keys[3] == d && m_keys[4] == e) {
        zero();
        return true;
    }
    return false;
}

} // namespace Penumbra::Script
