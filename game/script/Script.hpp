#pragma once

// THE ORIGINAL'S SCRIPT MODULE, DECLARED. main.as #included the other 24 .as
// files, so the game was ONE AngelScript module in which every constant,
// global, class and function was visible to every file. This header is that
// module's symbol table: each ported .cpp includes it and defines the part its
// .as declared. Grouped by the .as that declared each symbol, with the .cpp
// that must define it.
//
// Mapping (docs/spec/30-ethanon-runtime.md section 5, eth/Eth.hpp):
//   ETHEntity@ x                 ETHEntity x (a nullable handle, by value)
//   const string s               const string& s
//   float &out bias              float& bias
//   int[] a(2, 0)                array<int> a(2, 0)
//   dictionary of Foo@           dictionary<Foo>, holding std::shared_ptr<Foo>
//   frameTimer@ t                std::shared_ptr<frameTimer> t
//   ETHEntityArray@ f()          ETHEntityArray f() (by value)
//   main()                       ScriptMain()
//
// Class members: a class's PRIVATE data members are the original's names with
// the engine's m_ prefix (blinkTime -> m_blinkTime, dict -> m_dict); the
// public fields of the plain record Message keep their names (msg.alpha). Each
// member starts at the value the original's constructor gave it. A constructor
// that only assigned constants is defaulted HERE (do not define it); one that
// did work or takes arguments is declared here and defined in the .cpp named
// on its class.

#include <cstdint>
#include <memory>
#include <string>

#include "eth/Eth.hpp"

namespace Penumbra::Script {

using namespace Penumbra::Eth;

// --- Helpers the ported files share --------------------------------------------

// AngelScript's float -> uint8 conversion, as in `ARGB(uint8(alpha*255.0f),0,0,0)`
// (util.as:385, controlCharacters.as:458, environment.as:99): truncate toward
// zero to a 32-bit int (x86 cvttss2si), then keep the low 8 bits. cvttss2si
// gives 0x80000000 for NaN and for values outside int's range, and that low
// byte is 0 - so those return 0 (a NaN fails both comparisons). An int or uint
// to uint8 needs no helper: static_cast<uint8> is AngelScript's truncation.
inline uint8 ToUint8(float f)
{
    if (!(f > -2147483648.0f && f < 2147483648.0f)) {
        return 0;
    }
    return static_cast<uint8>(static_cast<int>(f) & 0xFF);
}

// AngelScript's float division checks its divisor: a 0 (or -0) raises "Divide
// by zero", which aborts the script function and unwinds the whole callback
// that called it, where C++ would give inf or NaN. machine.exe's asBC_DIVf
// handler compares the divisor with 0 (fucom) and jumps to the exception at
// 0x4a5358 on equality (machine.exe@0x4a45a6-0x4a45bb); a NaN divisor compares
// unordered and divides. Use it wherever a run-time float divisor can be 0
// (interface.as:59-61).
inline float DivF(float numerator, float divisor)
{
    if (divisor == 0.0f) {
        throw ScriptException("Divide by zero");
    }
    return numerator/divisor;
}

// main.as:124's main(), renamed: the Machine boots it (Machine::Boot) before
// the first frame. Defined in main.cpp.
void ScriptMain();

// Registers every ETHCallback_* under the name the runtime binds it by (the
// entity file name without ".ent") and every function a LoadScene names as a
// preLoop or loop. Defined in Register.cpp.
void RegisterAll(Machine& machine);

// === constants.as (functions defined in constants.cpp) ===========================

inline constexpr float PI = 3.141592654f;             // constants.as:43
inline constexpr float PI2 = 3.141592654f * 2;        // constants.as:44
inline constexpr float PIb = 1.570796327f;            // constants.as:45

inline const string endl = "\n";                      // constants.as:47
inline const string APPLICATION_TITLE = "Penumbra e o Castelo das Sombras - Ethanon Engine";   // constants.as:48

inline constexpr float GRAVITY = 1200.0f;             // constants.as:50

// The AI's `action` custom data (constants.as:52-54).
inline constexpr uint STANDING = 0;
inline constexpr uint CHASING = 1;
inline constexpr uint COOLDOWN = 2;

inline constexpr uint DEAD_FADE_OUT_TIME = 3000;      // constants.as:56
inline constexpr uint LIVE_FADE_IN_TIME = 3000;       // constants.as:57

inline constexpr uint PVP_MODE = 1;                   // constants.as:59
inline constexpr uint CAMPAIGN = 2;                   // constants.as:60

inline constexpr float DEFAULT_CHARSPRITE_CUTX = 4.0f;   // constants.as:62
inline constexpr float DEFAULT_CHARSPRITE_CUTY = 4.0f;   // constants.as:63

inline constexpr float SIZE_TOLERANCE = 76;           // constants.as:65

inline constexpr uint ATTACK_MODE_PHYSICAL = 0;       // constants.as:67
inline constexpr uint ATTACK_MODE_FIRE = 1;           // constants.as:68

inline constexpr uint MAX_PLAYERS = 2;                // constants.as:70
inline constexpr int MAX_PVP_POINTS = 3;              // constants.as:71

string actionToString(uint action);                   // constants.as:73

inline const string MAIN_CHARACTER_ENTITY0 = "bruxo.ent";      // constants.as:87
inline const string MAIN_CHARACTER_ENTITY1 = "princess.ent";   // constants.as:88

bool isAMainCharacter(ETHEntity thisEntity);          // constants.as:90

// === util.as (defined in util.cpp) ===============================================

ETHEntity findAmongNeighbourEntities(ETHEntity thisEntity, const string& entityName, bool horizontalOnly);   // util.as:43
ETHEntity findMainCharNeighbours(ETHEntity thisEntity, bool horizontalOnly);   // util.as:70
bool isInScreen(ETHEntity thisEntity);                // util.as:96
bool isInScreen(const vector2& pos);                  // util.as:112
collisionBox getAbsoluteCollisionBox(ETHEntity thisEntity);   // util.as:127
float getLength(const vector2& v);                    // util.as:134
float getDist(const vector2& v0, const vector2& v1);  // util.as:139
float getSquaredDist(const vector2& v0, const vector2& v1);   // util.as:144
bool isOnSight(ETHEntity thisEntity, ETHEntity other, float mult);   // util.as:150
void knockBack(ETHEntity thisEntity, const vector2& v);   // util.as:161
vector2 getKnockBackVector(ETHEntity thisEntity);     // util.as:169
void setKnockBackVector(ETHEntity thisEntity, const vector2& v);   // util.as:174
void applyForce(ETHEntity thisEntity, const vector2& v);   // util.as:180
// Never call unqualified with a std type as the argument (std::move by ADL).
void move(ETHEntity thisEntity);                      // util.as:188
vector2 getCurrentForce(ETHEntity thisEntity);        // util.as:221
void setForceX(ETHEntity thisEntity, float f);        // util.as:228
void setForceY(ETHEntity thisEntity, float f);        // util.as:233

// Directions, as findDirection returns them and the `currentDir` / `direction`
// custom data hold them (util.as:238-241).
inline constexpr uint RIGHT = 0;
inline constexpr uint LEFT = 1;
inline constexpr uint DOWN = 2;
inline constexpr uint UP = 3;

uint findDirection(const vector2& v);                 // util.as:242
uint findBoxDirection(const collisionBox& a, const collisionBox& b);   // util.as:267
string directionToString(uint dir);                   // util.as:305
uint findDirection(const vector3& v);                 // util.as:321
ETHEntityArray findDestinationBuckets(ETHEntity thisEntity, const vector2& dir, bool forceBottomBuckets);   // util.as:326
bool checkBoxHit(collisionBox box0, collisionBox box1);   // util.as:350
void drawRect(uint rectColor);                        // util.as:374
bool fadeIn(uint startTime);                          // util.as:379
bool fadeOut(uint startTime, float& bias);            // util.as:392 (bias is &out: unwritten when it returns true)
void addToExp(uint player, int exp);                  // util.as:406
// ENHANCEMENT E7 (not in the original): the experience a level needs,
// data.enml's global.lv<level>, or - where data.enml has no such key - the
// last level below it that has one. The original read the key directly and a
// missing one left the value unwritten: lv20 is missing, so level 19 skipped
// straight past 20, and past lv30 addToExp's loop never ended (docs/spec/12
// :864, :1111). Used by addToExp and drawPlayerStatus.
int expForLevel(int level);

// ENHANCEMENT E11 (not in the original): doCharacterCollision's "still on the
// ground" test counts every floor box collided this frame, not only the last,
// so the wizard no longer turns airborne for a frame at a seam between two
// floor tiles (controlCharacters.as:126, :192). false = the original.
inline constexpr bool kFloorSeamFix = true;

// ENHANCEMENT E15 (not in the original): setupScene also collects the ambient
// sound markers the level designer named "play_sound.ent" instead of
// "play_sound" (setupScene.as:175), so level 2's and 3's five extra
// horror.mp3 cues play. false = the original, where they are silent.
inline constexpr bool kPlaySoundEntFix = true;
void addToHp(ETHEntity thisEntity, int value);        // util.as:420
void addToMp(ETHEntity thisEntity, int value);        // util.as:435
void shadowText(const vector2& pos, const string& text, const string& font, float size,
                uint8 a, uint8 r, uint8 g, uint8 b);  // util.as:450
void loadingMessage();                                // util.as:457
ETHEntity findEntityInScreen(const string& name);     // util.as:462

// === eth_util.as (defined in eth_util.cpp) =======================================

string Vector3ToString(const vector3& v3);            // eth_util.as:49
string Vector2ToString(const vector2& v2);            // eth_util.as:55
string FormatToString(PIXEL_FORMAT format);           // eth_util.as:61

// eth_util.as:74-122. Places an input area on screen where the user can type
// texts. Constructor defaulted here (do not define it); PlaceInput and
// GetString are defined in eth_util.cpp.
class stringInput {
public:
    stringInput() = default;                          // eth_util.as:76-81
    void PlaceInput(const string& sText, const vector2& pos, const string& sFont,
                    float fSize, uint dwColor);       // eth_util.as:82
    string GetString();                               // eth_util.as:113

private:
    uint m_blinkTime = 300;                           // eth_util.as:78
    uint m_lastBlink = 0;                             // eth_util.as:79
    uint m_showingCarret = 1;                         // eth_util.as:80
    string m_ss;
};

// eth_util.as:128-167. Helps handling keyframe animation. Constructor defaulted
// here (do not define it); Get and Set are defined in eth_util.cpp.
class frameTimer {
public:
    frameTimer() = default;                           // eth_util.as:130-134
    uint Get();                                       // eth_util.as:136
    uint Set(uint nFirst, uint nLast, uint nStride);  // eth_util.as:141

private:
    uint m_lastTime = 0;
    uint m_currentFirst = 0;
    uint m_currentLast = 0;
    uint m_currentFrame = 0;
};

// === cameraManager.as (methods defined in cameraManager.cpp) =====================

// cameraManager.as:43-159. Constructor defaulted here (do not define it).
class CameraManager {
public:
    CameraManager() = default;                        // cameraManager.as:45-58
    void setCameraSpeed(float speed);                 // cameraManager.as:60
    void setMainCharPos(const vector2& v, uint player);   // cameraManager.as:65
    vector2 getMainCharPos(uint player) const;        // cameraManager.as:73
    void startEarthquake(float force);                // cameraManager.as:106
    void adjustCameraPos(bool pvpMode);               // cameraManager.as:111

private:
    void doEarthquake();                              // cameraManager.as:82
    void roundUpCameraPos();                          // cameraManager.as:143

    [[maybe_unused]] uint m_accelerationTime = 20000; // cameraManager.as:57 (never read)
    [[maybe_unused]] uint m_inBoundsLastTime = 0;     // cameraManager.as:56 (never read)
    vector2 m_mainCharPos0{0.0f, 0.0f};               // cameraManager.as:47
    vector2 m_mainCharPos1{0.0f, 0.0f};               // cameraManager.as:48
    vector2 m_screenLimit{0.4f, 0.3f};                // cameraManager.as:49
    float m_cameraSpeed = 150.0f;                     // cameraManager.as:50
    float m_earthquake = 0.0f;                        // cameraManager.as:51
    uint m_lastTremble = 0;                           // cameraManager.as:52
    uint m_trembleInterval = 70;                      // cameraManager.as:53
    bool m_eqForth = true;                            // cameraManager.as:54
    float m_backValue = 0;                            // cameraManager.as:55
};

// === messageManager.as (methods defined in messageManager.cpp) ===================

// messageManager.as:43-57. A plain record (held by handle in MessageManager's
// dictionary, by value in its damage array), so its fields are public and keep
// their names. Constructor defaulted here (do not define it).
class Message {
public:
    Message() = default;                              // messageManager.as:45-51
    uint time = 0;
    string message;                                   // ""
    uint8 alpha = 0;
    vector2 origin{0.0f, 0.0f};
};

// messageManager.as:59-220. Constructor DECLARED here and defined in
// messageManager.cpp: it resizes m_damage to m_maxDamageMessages
// (messageManager.as:66); the constants it assigned are the initializers below.
class MessageManager {
public:
    MessageManager();                                 // messageManager.as:61-68
    void addMessage(int damageValue, const vector2& origin);   // messageManager.as:70
    void addMessage(const string& msg);               // messageManager.as:82
    // `CameraManager@ camera` in the original; its one caller passes @g_camera
    // (setupScene.as:333), never null, so it is a reference here.
    void showMessages(const vector2& pos, const string& font, float size,
                      uint8 r, uint8 g, uint8 b, CameraManager& camera);   // messageManager.as:128
    string getKey(uint n);                            // messageManager.as:208

private:
    void processMessages();                           // messageManager.as:177

    dictionary<Message> m_dict;                       // AngelScript's dict.delete(k) is m_dict.deleteKey(k)
    uint m_maxMessages = 10;                          // messageManager.as:63
    uint m_messageTime = 4000;                        // messageManager.as:65
    string m_lastMessage;
    array<Message> m_damage;                          // resized to 15 by the constructor
    uint m_damageIndex = 0;                           // messageManager.as:67
    uint m_maxDamageMessages = 15;                    // messageManager.as:64
};

// === timer.as (defined in timer.cpp) =============================================

string getTimeString(uint time);                      // timer.as:43

// timer.as:54-80. Constructor defaulted here (do not define it).
class Timer {
public:
    Timer() = default;                                // timer.as:56-59
    void start();                                     // timer.as:61
    uint getElapsedTime();                            // timer.as:66
    void showTimer(const vector2& pos, float size, uint8 a,
                   uint8 r, uint8 g, uint8 b);        // timer.as:71

private:
    uint m_startTime = 0;                             // timer.as:58
};

// === combo.as (methods defined in combo.cpp) =====================================

inline constexpr uint MAX_COMBO_KEYS = 5;             // combo.as:43
inline constexpr uint BUTTON_STRIDE = 210;            // combo.as:44

// Combo commands (combo.as:46-52).
inline constexpr uint CMD_NONE = 0;
inline constexpr uint CMD_UP = 1;
inline constexpr uint CMD_DOWN = 2;
inline constexpr uint CMD_LEFT = 3;
inline constexpr uint CMD_RIGHT = 4;
inline constexpr uint CMD_SWORD = 5;
inline constexpr uint CMD_SPELL = 6;

// combo.as:54-155. Constructor DECLARED here and defined in combo.cpp: it
// resizes m_keys to MAX_COMBO_KEYS (combo.as:58).
class Combo {
public:
    Combo();                                          // combo.as:56-61
    void updateInput(uint player);                    // combo.as:63
    bool checkSequence(uint a, uint b, uint c);       // combo.as:131
    bool checkSequence(uint a, uint b, uint c, uint d, uint e);   // combo.as:141

private:
    void zero();                                      // combo.as:122

    array<uint> m_keys;
    uint m_lastInput = 0;                             // combo.as:59
    uint m_index = 0;                                 // combo.as:60
};

// === switch.as (methods defined in switch.cpp) ===================================

// switch.as:43-117. A two-way option on screen (text, or text and an image).
// BOTH constructors are DECLARED here and defined in switch.cpp; videoModes.as:43-45
// uses the 2-argument form twice and the 4-argument form once.
class Switch {
public:
    Switch(const string& b0, const string& b1);       // switch.as:45-51
    Switch(const string& b0, const string& img0, const string& b1, const string& img1);   // switch.as:53-63
    void put(const vector2& pos, const string& font, float size, float width);   // switch.as:65
    uint getCurrent() const;                          // switch.as:104
    void setCurrent(uint newCurrent);                 // switch.as:109

private:
    uint m_current = 0;                               // switch.as:47 / :55
    array<string> m_button;
    array<string> m_image;                            // empty for a text-only switch
};

// ENHANCEMENT E10 (not in the original): a value in `steps` equal steps from 0     // E10
// to 100%, on the options screen below the Switches and in their font, colour     // E10
// and alphas: "label  [<] 70% [>]". Hovering an arrow and confirming moves it     // E10
// one step, and it stops at either end rather than wrapping: a volume that        // E10
// wrapped from 100% to 0% on one click too many would be a trap. Constructor      // E10
// and methods defined in switch.cpp.                                              // E10
class Stepper {                                       // E10
public:                                               // E10
    Stepper(const string& label, uint steps, uint current);   // E10: steps at least 1, current clamped
    // The label at `pos`, the arrows and the value from pos.x+labelWidth.       // E10
    void put(const vector2& pos, const string& font, float size, float labelWidth);   // E10
    uint getCurrent() const;                          // E10: 0..steps
    void setCurrent(uint newCurrent);                 // E10: clamped to steps
    uint getSteps() const;                            // E10
    // For the layer's 0..1 settings (volumes): the value as a fraction, and    // E10
    // the nearest step to a fraction (0 below 0 and for NaN, steps above 1).    // E10
    float getFraction() const;                        // E10
    uint stepFor(float fraction) const;               // E10

private:                                              // E10
    string m_label;                                   // E10
    uint m_steps = 10;                                // E10
    uint m_current = 10;                              // E10
};                                                    // E10

// ENHANCEMENT E23 (not in the original): one value out of a list the layer       // E23
// gives it, on the options screen in the Stepper's font, colour and alphas: the    // E23
// label on its own line, and under it "[<] value [>]" - the arrows in boxes as    // E23
// the Stepper's, the value in a box of the width put() is given. Hovering an     // E23
// arrow and confirming moves one option; it stops at either end, as the Stepper  // E23
// does. Constructor and methods defined in switch.cpp.                           // E23
class Chooser {                                       // E23
public:                                               // E23
    explicit Chooser(const string& label);            // E23
    // The options, drawn as they are (cp1252, translated at the draw boundary), // E23
    // and which is current (clamped).                                         // E23
    void setOptions(const array<string>& options, uint current);   // E23
    void put(const vector2& pos, const string& font, float size, float valueWidth);   // E23: two lines from pos
    uint getCurrent() const;                          // E23
    void setCurrent(uint newCurrent);                 // E23: clamped to the options
    uint getCount() const;                            // E23

private:                                              // E23
    string m_label;                                   // E23
    array<string> m_options;                          // E23
    uint m_current = 0;                               // E23
};                                                    // E23

// === scores.as (defined in scores.cpp) ===========================================

inline constexpr uint MAX_SCORES = 5;                 // scores.as:43

bool addNewRecordTime(uint elapsed);                  // scores.as:45
string getRecordTimeList();                           // scores.as:92
uint getGetBestTime();                                // scores.as:110

// === main.as (globals and functions defined in main.cpp) =========================

extern dictionary<frameTimer> g_frameTimers;          // main.as:43, keyed "id"+GetID()
extern enmlFile g_gameData;                           // main.as:44
extern ETHEntityArray g_spawn;                        // main.as:45
extern ETHEntityArray g_sounds;                       // main.as:46
extern MessageManager g_messages;                     // main.as:47
extern CameraManager g_camera;                        // main.as:48
extern Timer g_timer;                                 // main.as:49

extern int g_lives;                                   // main.as:51  = 0
extern uint g_numNpcs;                                // main.as:52  = 0
extern uint g_levelStartTime;                         // main.as:53  = 0
extern bool g_gameFinished;                           // main.as:54  = false
extern array<int> g_exp;                              // main.as:55  (2, 0)
extern array<int> g_charLevel;                        // main.as:56  (2, 8) - 8, not 1: resetData sets 1
extern uint g_newRecordTime;                          // main.as:57  = 0
extern array<bool> g_castingLight;                    // main.as:58  (2, false)
extern array<int> g_pvpPoints;                        // main.as:59  (2, 0)
extern array<Combo> g_comboManager;                   // main.as:60  (2)

void resetData();                                     // main.as:62
void newGame(const string& sceneName);                // main.as:99
// main() is ScriptMain(), declared at the top.       // main.as:124
void ETHCallback_help(ETHEntity thisEntity);          // main.as:147
void ETHCallback_story(ETHEntity thisEntity);         // main.as:163
void ETHCallback_checkpoint(ETHEntity thisEntity);    // main.as:170
void ETHCallback_next_level(ETHEntity thisEntity);    // main.as:194

// === gameover.as (defined in gameover.cpp) =======================================

extern uint g_gameOverStartTime;                      // gameover.as:43  = 0

void gameOverPreLoop();                               // gameover.as:45
void gameOverLoop();                                  // gameover.as:62

// === videoModes.as (defined in videoModes.cpp) ===================================

// Defined in videoModes.cpp exactly as videoModes.as:43-45 constructs them:
//   g_enablePS("Ativa pixel shaders", "Desativa pixel shaders")
//   g_windowed("Janela", "Tela-cheia")
//   g_controls("2\xBA joystick para jogador 2", "interface/input_options1.png",
//              "1\xBA joystick para jogador 1", "interface/input_options2.png")
extern Switch g_enablePS;                             // videoModes.as:43
extern Switch g_windowed;                             // videoModes.as:44
extern Switch g_controls;                             // videoModes.as:45

// ENHANCEMENT E10 (not in the original): the enhanced settings' rows on the     // E10
// same screen. The scripts never read them: PenumbraLayer seeds them from the   // E10
// settings when it attaches and, every tick, saves and applies what changed.    // E10
extern Switch g_keyboardP2;                           // E10: 0 = keyboard player 2 on (E4), 1 = off
extern Switch g_widescreen;                           // E10: 0 = widescreen levels (E1), 1 = 4:3
extern Switch g_language;                             // E10: 0 = Portuguese, 1 = English (E5)
extern Stepper g_musicVolume;                         // E10: tenths of the music's master volume
extern Stepper g_effectsVolume;                       // E10: tenths of the effects' master volume
extern Switch g_smoothMotion;                         // E10: 0 = smooth motion on (E8), 1 = off
extern Switch g_pauseOnFocusLoss;                     // E13: 0 = pause on focus loss, 1 = play on

// ENHANCEMENT E20 (not in the original): the options screen on a phone. The    // E20
// layer raises g_mobileLayout when it attaches (PENUMBRA_MOBILE builds); the     // E20
// scripts then leave out what a phone has no use for - the video-mode list,     // E20
// g_windowed's switch and the Alt+Enter line (showToggleFullscreenMessage) -    // E20
// and draw g_touchControls, E16's on/off, where g_windowed's switch was. A       // E20
// runtime flag rather than a build one, so the desktop suites can run the        // E20
// phone's screen; left down, every screen is drawn exactly as before.           // E20
extern bool g_mobileLayout;                           // E20
extern Switch g_touchControls;                        // E20: 0 = touch controls on (E16), 1 = off

// ENHANCEMENT E23 (not in the original): the display mode, chosen for the      // E23
// player or by them. The mode list's first line is "Autom\xE1tico (melhor)",     // E23
// which sends SetWindowProperties 0 x 0 (automatic, in a window or in           // E23
// fullscreen, render/WindowMode.hpp); the lines are "WxH" (videoModeToString's  // E23
// "WxHx32" named a bit depth nothing chooses any more), the monitor's native    // E23
// size marked " (nativa)", and the current choice marked with the Switch rows'  // E23
// "[\x95]". g_refreshRate is the rate fullscreen runs at (on a phone, the       // E23
// display's). The layer sets all four: the scripts never choose a mode.         // E23
extern videoMode g_chosenVideoMode;                   // E23: the list's marked size; 0 x 0 = the automatic line
extern videoMode g_nativeVideoMode;                   // E23: the size marked " (nativa)"; 0 x 0 = none
extern Chooser g_refreshRate;                         // E23: the layer's options, 0 = automatic
extern bool g_refreshRateRow;                         // E23: false leaves the row out (iOS: nothing sets a rate there)
string videoModeLabel(const videoMode& vm);           // E23: "WxH", and " (nativa)" for the native size

string videoModeToString(const videoMode& vm);        // videoModes.as:47
void screenModesPreLoop();                            // videoModes.as:52
void ETHCallback_picker(ETHEntity thisEntity);        // videoModes.as:58
bool putBackButton(const vector2& cursor);            // videoModes.as:65
void screenModesLoop();                               // videoModes.as:85

// === menu.as (defined in menu.cpp) ===============================================

void loopMenuSong();                                  // menu.as:43
void menuPreLoop();                                   // menu.as:50
void detectJoysticks();                               // menu.as:74
void menuLoop();                                      // menu.as:92
void showToggleFullscreenMessage();                   // menu.as:97

// The panel texts, as the bytes AngelScript 2.20.0 (machine.exe's) made of
// menu.as's heredocs: the file is read in binary, so its CRLFs stay; the first
// line (whitespace up to the first \n) is removed; the LAST \n is removed but
// the \r before it stays, so each ends in a lone \r (as_compiler.cpp 2.20.0,
// ProcessHeredocStringConstant). No escapes are processed inside a heredoc.
// 0x95 is cp1252's bullet.
inline const string como_jogar =                      // menu.as:123-149
    "\x95" "Controles\r\n"
    " Movimento: < e >\r\n"
    " Pulo: ^ ou CTRL (joystick 3)\r\n"
    " Ataque/espada: S (joystick 4)\r\n"
    " Ataque/fogo: D (joystick 2)\r\n"
    " Acionar Luz: ESPA\xC7O (joystick 1)\r\n"
    "\r\n"
    " Detectar joysticks: segure J\r\n"
    "\r\n"
    "\x95" "2 jogadores:\r\n"
    " Se houver um joystick plugado,\r\n"
    " o personagem principal ganha \r\n"
    " a habilidade de invocar a\r\n"
    " criatura que pode ser controlada\r\n"
    " pelo 2\xBA jogador.\r\n"
    "\r\n"
    " Para invocar a criatura basta pres-\r\n"
    " sionar START no 2\xBA controle.\r\n"
    "\r\n"
    " A m\xE1gica de invoca\xE7\xE3o custa 50 mana\r\n"
    " e uma vida, mas possibilitar\xE1 que 2\r\n"
    " jogadores lutem lado-a-lado.\r\n"
    "\r\n"
    " Somente o jogador 1 pode pegar\r\n"
    " checkpoints e passar de fase.\r";

inline const string config =                          // menu.as:151-157
    "Ajuste as op\xE7\xF5" "es de v\xED" "deo como\r\n"
    "resolu\xE7\xE3o de tela e uso de shaders.\r\n"
    "\r\n"
    "Configure tamb\xE9m o uso do joystick\r\n"
    "pelo segundo jogador.\r";

inline const string creditos =                        // menu.as:159-184
    "Andr\xE9 Santee\r\n"
    " -Programa\xE7\xE3o\r\n"
    " -Scripting e mec\xE2nica do jogo\r\n"
    " -Efeitos especiais\r\n"
    " -Game design\r\n"
    "\r\n"
    "Arthur Santee\r\n"
    " -Modelagem 3D\r\n"
    " -Game design\r\n"
    "\r\n"
    "Gabriel Duarte\r\n"
    " -Trilha sonora\r\n"
    "  gabrielduarte.wordpress.com\r\n"
    "\r\n"
    "Approaching Thunderstorm\r\n"
    " -www.freesoundtrackmusic.com\r\n"
    "\r\n"
    "Agradecimentos especiais:\r\n"
    "-James Hastings-Trew por ter nos\r\n"
    " cedido algumas de suas texturas\r\n"
    "  planetpixelemporium.com\r\n"
    "-Jos\xE9 Rodolfo Ortale\r\n"
    "-Rafael \"Pet\" Alencar\r\n"
    "-Taina Monclaire\r";

// ENHANCEMENT E21: the enhanced edition's credit, drawn after creditos as a
// block of its own, in the original's format (a name, then " -" roles):
// menu.as credited the 2010 team only, and those lines stay theirs, untouched.
// It begins with "\n" because creditos ends in the lone CR AngelScript left:
// CR LF is one break, so "\n\r\n" makes exactly one blank line. No CR at the
// end: FontAtlas counts one as a line. The panel has room for exactly three
// more lines - showData draws from y 70 in Arial Narrow 25, 25 px a line, the
// original's 24 end at 670, these three at 745 of 768 - so one role, not
// several. 0x8D is the port's byte for U+0107, c with acute (eth/Text.hpp).
// strings.json's "patterns" has the English (the original's credits, then this).
inline const string creditosEnhanced =
    "\n\r\n"
    "Ivan Cvetanovi\x8D\r\n"
    " -Edi\xE7\xE3o aprimorada (Supersonic Engine)";

inline const string novo_jogo =                       // menu.as:186-207
    "Penumbra n\xE3o \xE9 um bom lugar\r\n"
    "para se viver. Quando acaba a\r\n"
    "neblina, chega a tempestade.\r\n"
    "\r\n"
    "Eras atr\xE1s, um mago muito\r\n"
    "poderoso tomou o controle da\r\n"
    "sombria terra chamada Penumbra,\r\n"
    "nomeando-se o rei deste mundo.\r\n"
    "\r\n"
    "Por diversos s\xE9" "culos, a Ordem dos\r\n"
    "Bruxos de Penumbra t\xEAm buscado\r\n"
    "derrubar o tirano.\r\n"
    "\r\n"
    "Ap\xF3s incont\xE1veis anos treinando\r\n"
    "as artes da m\xE1gica e da luta voc\xEA\r\n"
    "foi escolhido pela Ordem para \r\n"
    "ir ao Castelo das Sombras\r\n"
    "de Penumbra e assassinar o rei,\r\n"
    "acabando com um reinado que dura\r\n"
    "anos.\r";

// menu.as:209-215: ordinary literals (\n escapes, no CRs) joined with
// MAX_PVP_POINTS. std::to_string rather than Str() because this is initialised
// before main, and it gives the same "3" for an int.
inline const string versus =
    string("Escolha uma arena e dispute uma\n"
           "partida contra outro jogador.\n\n"
           "-Quem derrotar o outro ganha 1 ponto\n"
           "-Vence quem fizer ") + std::to_string(MAX_PVP_POINTS) + " pontos primeiro\n"
    "-A partida acaba se a diferen\xE7" "a no\n"
    "placar exceder " + std::to_string(MAX_PVP_POINTS) + " pontos";

void showData(const string& title, const string& content);   // menu.as:217
void ETHCallback_cursor(ETHEntity thisEntity);        // menu.as:232
void ETHCallback_thumbnail(ETHEntity thisEntity);     // menu.as:362
bool waitForInputToMenu();                            // menu.as:374
bool goToMenu();                                      // menu.as:383
bool escToGoToMenu();                                 // menu.as:393

// === playerInput.as (defined in playerInput.cpp) =================================

uint getPlayerJoystick(uint player);                  // playerInput.as:43
KEY_STATE getLeftButtonStatus(uint player);           // playerInput.as:55
KEY_STATE getRightButtonStatus(uint player);          // playerInput.as:75
KEY_STATE getUpButtonStatus(uint player);             // playerInput.as:95
KEY_STATE getDownButtonStatus(uint player);           // playerInput.as:115
vector2 getPlayerXYAxis(uint player);                 // playerInput.as:135
bool hasASecondController();                          // playerInput.as:159
uint getInputDirection(uint player);                  // playerInput.as:166
KEY_STATE getJumpButtonStatus(uint player);           // playerInput.as:176
KEY_STATE getAttack01ButtonStatus(uint player);       // playerInput.as:201
KEY_STATE getAttack02ButtonStatus(uint player);       // playerInput.as:221
KEY_STATE getAttack03ButtonStatus(uint player);       // playerInput.as:241
KEY_STATE getConfirmButtonStatus(uint player);        // playerInput.as:261
KEY_STATE getCancelButtonStatus(uint player);         // playerInput.as:289
void controlCharacter(ETHEntity thisEntity, uint player);   // playerInput.as:309

// === characterAI.as (defined in characterAI.cpp) =================================

void rangedCharacterAI(ETHEntity thisEntity, const string& effectEntity, float speedMultiplier);   // characterAI.as:43
void meleeCharacterAI(ETHEntity thisEntity, const string& swordEffect);   // characterAI.as:117
void mixedCharacterAI(ETHEntity thisEntity, const string& meleeFx, const string& rangedFx, float distance,
                      float speedMultiplier);         // characterAI.as:201

// === controlCharacters.as (defined in controlCharacters.cpp) =====================

void doCharacterCollision(ETHEntity thisEntity, bool useNpcInvisibleWalls);   // controlCharacters.as:43
void animateCharacter(ETHEntity thisEntity, std::shared_ptr<frameTimer> timer);   // controlCharacters.as:222
void ETHCallback_bruxo(ETHEntity thisEntity);         // controlCharacters.as:270
void ETHCallback_princess(ETHEntity thisEntity);      // controlCharacters.as:304
bool isDead(ETHEntity thisEntity, const string& beamEffect);   // controlCharacters.as:369
bool isMainCharDead(ETHEntity thisEntity, const string& beamEffect);   // controlCharacters.as:406
void ETHCallback_warrior(ETHEntity thisEntity);       // controlCharacters.as:471
void ETHCallback_minion(ETHEntity thisEntity);        // controlCharacters.as:495
void ETHCallback_knight(ETHEntity thisEntity);        // controlCharacters.as:519
void ETHCallback_impy(ETHEntity thisEntity);          // controlCharacters.as:543
void ETHCallback_master_knight(ETHEntity thisEntity); // controlCharacters.as:567
void ETHCallback_paladin(ETHEntity thisEntity);       // controlCharacters.as:591
void summoner(ETHEntity thisEntity, const string& creature,
              const string& summonOriginEntity, uint interval);   // controlCharacters.as:615
void ETHCallback_king(ETHEntity thisEntity);          // controlCharacters.as:645
void player1Summoner(ETHEntity player0);              // controlCharacters.as:674

// === doDamage.as (defined in doDamage.cpp) =======================================

// knockFromOwner: if true, the "knock back" will push the target from the
// attacker's position instead of pushing from the hit entity effect.
ETHEntity doDamage(ETHEntity thisEntity, float knockForce, bool knockFromOwner,
                   bool collideEverything, uint attackMode, float damageMultiplier);   // doDamage.as:45

// === swords.as (defined in swords.cpp) ===========================================

bool swordAttack(ETHEntity thisEntity, const string& swordName, const string& beamName, bool isInRange);   // swords.as:43
void ETHCallback_enemy_sword(ETHEntity thisEntity);   // swords.as:109
void ETHCallback_dark_sword(ETHEntity thisEntity);    // swords.as:114
void ETHCallback_sword0(ETHEntity thisEntity);        // swords.as:120
void ETHCallback_sword1(ETHEntity thisEntity);        // swords.as:124
void ETHCallback_combo_sword(ETHEntity thisEntity);   // swords.as:128
void ETHCallback_paladin_sword(ETHEntity thisEntity); // swords.as:133

// === interface.as (defined in interface.cpp) =====================================

void drawPlayerStatus(ETHEntity thisEntity);          // interface.as:43
void doMpRecovery(ETHEntity thisEntity, uint stride, int incr);   // interface.as:98

// === spells.as (defined in spells.cpp) ===========================================

bool castSpell(ETHEntity owner, const string& spellName, int mana, const vector3& spellOffset,
               int damage, float speedMultiplier);    // spells.as:43
void ETHCallback_light_spell(ETHEntity thisEntity);   // spells.as:83
void ETHCallback_fire_ball(ETHEntity thisEntity);     // spells.as:113
void ETHCallback_combo_fire_ball(ETHEntity thisEntity);   // spells.as:118
void manageFireBall(ETHEntity thisEntity, const string& explosion);   // spells.as:123

// === potions.as (defined in potions.cpp) =========================================

void doPotion(ETHEntity thisEntity);                  // potions.as:43
void ETHCallback_potion(ETHEntity thisEntity);        // potions.as:63
void ETHCallback_potion_small(ETHEntity thisEntity);  // potions.as:68
void ETHCallback_potion_large(ETHEntity thisEntity);  // potions.as:73

// === lavaShooter.as (defined in lavaShooter.cpp) =================================

void ETHCallback_shooter(ETHEntity thisEntity);       // lavaShooter.as:43
void ETHCallback_fire_shoot(ETHEntity thisEntity);    // lavaShooter.as:64

// === environment.as (defined in environment.cpp) =================================

void ETHCallback_clouds(ETHEntity thisEntity);        // environment.as:43
void ETHCallback_fog(ETHEntity thisEntity);           // environment.as:115
void positionEnvironmentElements(ETHEntity thisEntity);   // environment.as:122
void ETHCallback_dawn(ETHEntity thisEntity);          // environment.as:145

// === events.as (defined in events.cpp) ===========================================

void ETHCallback_event01(ETHEntity thisEntity);       // events.as:43
inline constexpr float SIZE_ADDITION = 6;             // events.as:66
void ETHCallback_falling_bridge(ETHEntity thisEntity);   // events.as:67

// === setupScene.as (defined in setupScene.cpp) ===================================

void ETHCallback_play(ETHEntity thisEntity);          // setupScene.as:43
void spawn(ETHEntity handle, const string& name);     // setupScene.as:52
void setupScene();                                    // setupScene.as:114
void levelLoop();                                     // setupScene.as:226
void pvpLoop();                                       // setupScene.as:231
void goToPvp();                                       // setupScene.as:242
void doLoop(bool pvp);                                // setupScene.as:248

} // namespace Penumbra::Script
