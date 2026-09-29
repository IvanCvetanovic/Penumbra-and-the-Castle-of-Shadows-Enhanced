// The top layer's text: game/data/strings.json against the original's real
// strings (every literal that reaches DrawText, the heredocs, data.enml's lore
// and arena texts, the scenes' help signs and arena titles), the composed
// strings through the pattern rules, the control hints' touch wording (E16:
// every text that names a key has it, in both languages, and fits where it is
// drawn; with touch off not a byte changes), FontAtlas's layout of a two-line
// cp1252 string with an accent (when the system has the fonts), the quads
// HudRenderer builds on a bare registry, and E1's wide menus (Step 25): the
// rectangles that go on past the screen's sides, the backdrop each fixed-layout
// scene gets, and the real main menu in a 16:9 window.
// Script.hpp first: an engine header that reaches <windows.h> would turn the
// script API's DrawText into DrawTextA.
#include "script/Script.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "TestHarness.hpp"
#include "eth/Defs.hpp"
#include "eth/Machine.hpp"
#include "eth/Snapshot.hpp"
#include "eth/Text.hpp"
#include "render/CameraRig.hpp"
#include "render/FontAtlas.hpp"
#include "render/HudRenderer.hpp"
#include "render/Localization.hpp"
#include "render/TextureCache.hpp"
#include "render/View.hpp"
#include "render/WideMenus.hpp"

using namespace Penumbra;
using Render::Language;

namespace {

const std::string kApp = PENUMBRA_ORIGINAL_DIR;

std::string ReadBytes(const std::string& path) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string WithoutCr(const std::string& text) {
    std::string out;
    for (const char c : text) {
        if (c != '\r') out += c;
    }
    return out;
}

// An AngelScript string literal's escapes, as the compiler read them.
std::string Unescape(const std::string& source) {
    std::string out;
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\\' && i + 1 < source.size()) {
            const char next = source[++i];
            out += next == 'n' ? '\n' : next == 't' ? '\t' : next;
            continue;
        }
        out += source[i];
    }
    return out;
}

// What `const string name = """ ... """;` held: AngelScript drops the first
// line when it is only whitespace, and the last likewise.
std::string Heredoc(const std::string& script, const std::string& name) {
    const std::string open = "const string " + name + " = \"\"\"";
    const std::size_t start = script.find(open);
    if (start == std::string::npos) return {};
    const std::size_t begin = start + open.size();
    const std::size_t end = script.find("\"\"\";", begin);
    if (end == std::string::npos) return {};
    std::string body = script.substr(begin, end - begin);
    std::size_t lead = 0;
    while (lead < body.size() && (body[lead] == ' ' || body[lead] == '\t')) ++lead;
    if (lead < body.size() && body[lead] == '\r') ++lead;
    if (lead < body.size() && body[lead] == '\n') body.erase(0, lead + 1);
    std::size_t tail = body.size();
    while (tail > 0 && (body[tail - 1] == ' ' || body[tail - 1] == '\t')) --tail;
    if (tail > 0 && body[tail - 1] == '\n') {
        --tail;
        if (tail > 0 && body[tail - 1] == '\r') --tail;
        body.erase(tail);
    }
    return body;
}

// data.enml's `key = value;`, read as enml.h read it: the whitespace before the
// value skipped, the value up to the ';'.
std::string EnmlValue(const std::string& enml, const std::string& key) {
    const std::size_t at = enml.find("\t" + key + " =");
    if (at == std::string::npos) return {};
    std::size_t pos = enml.find('=', at) + 1;
    while (pos < enml.size() && (enml[pos] == ' ' || enml[pos] == '\n' || enml[pos] == '\t')) ++pos;
    const std::size_t end = enml.find(';', pos);
    return end == std::string::npos ? std::string() : enml.substr(pos, end - pos);
}

std::string XmlText(std::string text) {
    const std::pair<const char*, const char*> entities[] = {
        {"&apos;", "'"}, {"&quot;", "\""}, {"&lt;", "<"}, {"&gt;", ">"}, {"&amp;", "&"}};
    for (const auto& [from, to] : entities) {
        for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + 1)) {
            text.replace(at, std::string(from).size(), to);
        }
    }
    return text;
}

// Every custom-data string named `name` in a scene file.
std::vector<std::string> SceneStrings(const std::string& scene, const std::string& name) {
    std::vector<std::string> out;
    const std::string tag = "<Name>" + name + "</Name>";
    for (std::size_t at = scene.find(tag); at != std::string::npos; at = scene.find(tag, at + 1)) {
        const std::size_t open = scene.find("<Value>", at);
        const std::size_t close = scene.find("</Value>", open);
        if (open == std::string::npos || close == std::string::npos) break;
        out.push_back(XmlText(scene.substr(open + 7, close - open - 7)));
    }
    return out;
}

void CheckTranslated(const Render::Localization& loc, const std::string& portuguese, const std::string& where) {
    const bool known = loc.HasTranslation(portuguese);
    CHECK_MSG(known, where + ": no English for \"" + portuguese + "\"");
}

// The literals of the .as files that reach DrawText (directly, through
// shadowText/showData, or as a MessageManager message), as written in the
// source: each is checked to be there, byte for byte, and to have English.
struct Literal {
    const char* file;
    const char* source;
};
const Literal kLiterals[] = {
    {"menu.as", "Pressione Alt+Enter para trocar entre fullscreen e modo janela"},
    {"menu.as", "Cr\xE9"
                "ditos"},
    {"menu.as", "Melhores tempos"},
    {"menu.as", "Como Jogar"},
    {"menu.as", "Jogador versus Jogador"},
    {"menu.as", "Novo jogo"},
    {"menu.as", "Sair do jogo"},
    {"menu.as", "Configura\xE7\xF5"
                "es"},
    {"menu.as", "\xC9 necess\xE1rio ao menos um joystick\\n para jogar neste modo."},
    {"controlCharacters.as", "A criatura invocada n\xE3o pode sair do campo de vis\xE3o da tela"},
    {"controlCharacters.as", "\xC9 necess\xE1rio de 50 mana para invocar a criatura"},
    {"controlCharacters.as", "N\xE3o \xE9 poss\xEDvel invocar 2 criaturas ao mesmo tempo"},
    {"controlCharacters.as", "Criatura m\xE1gica invocada"},
    {"controlCharacters.as", "Imposs\xEDvel invocar criatura daqui"},
    {"playerInput.as", "\xC9 necess\xE1rio ter 5 de mana para realizar este combo"},
    {"playerInput.as", "Esta m\xE1gica j\xE1 est\xE1 em execu\xE7\xE3o"},
    {"main.as", "Checkpoint..."},
    {"util.as", "Carregando...\\n"},
    {"setupScene.as", "Seu tempo total foi:"},
    {"videoModes.as", "Op\xE7\xF5"
                      "es de v\xED"
                      "deo"},
    {"videoModes.as", "Ativa pixel shaders"},
    {"videoModes.as", "Desativa pixel shaders"},
    {"videoModes.as", "Janela"},
    {"videoModes.as", "Tela-cheia"},
    {"videoModes.as", "2\xBA joystick para jogador 2"},
    {"videoModes.as", "1\xBA joystick para jogador 1"},
    {"constants.as", "Penumbra e o Castelo das Sombras - Ethanon Engine"},
};

// ENHANCEMENT E10: the options screen's own rows (game/script/videoModes.cpp),
// in the port's Portuguese, so not in any .as file and not in kLiterals.
const char* const kE10Labels[] = {
    "Teclado para o jogador 2",
    "Jogador 2 s\xF3 no joystick",
    "Tela larga (widescreen)",
    "Tela 4:3 (original)",
    "Vale a partir da pr\xF3xima fase",
    "Portugu\xEAs",
    "English",
    "Volume da m\xFAsica",
    "Volume dos efeitos",
    "Ativa movimento suave",   // E8's row
    "Desativa movimento suave",
    "Pausa ao perder o foco",   // E13's row
    "Continua sem o foco",
    "Ativa controles de toque",   // E20's row, on a phone
    "Desativa controles de toque",
};

void TestLocalization() {
    Render::Localization loc;
    CHECK(loc.Load());
    CHECK(loc.StringCount() > 60);
    CHECK(loc.PatternCount() >= 8);
    CHECK(loc.CurrentLanguage() == Language::English);

    // Every DrawText literal is real and has English.
    for (const Literal& literal : kLiterals) {
        const std::string script = ReadBytes(kApp + "/" + literal.file);
        const std::string quoted = std::string("\"") + literal.source + "\"";
        CHECK_MSG(script.find(quoted) != std::string::npos,
                  std::string(literal.file) + " does not hold " + quoted);
        const std::string text = Unescape(literal.source);
        CheckTranslated(loc, text, literal.file);
    }
    // E10: every label of the enhanced settings' rows has English, and the
    // switch rows and the stepper's pieces come out as drawn.
    for (const char* label : kE10Labels) CheckTranslated(loc, label, "videoModes.cpp (E10)");
    CHECK(loc.Translate("[\x95] Tela larga (widescreen)") == "[\x95] Widescreen");
    CHECK(loc.Translate("[ ] Jogador 2 s\xF3 no joystick") == "[ ] Player 2 on a joystick only");
    CHECK(loc.Translate("[ ] Portugu\xEAs") == "[ ] Portugu\xEAs");   // each language named in its own
    CHECK(loc.Translate("[\x95] English") == "[\x95] English");
    CHECK(loc.Translate("Volume da m\xFAsica") == "Music volume");
    CHECK(loc.Translate("[ ] Desativa movimento suave") == "[ ] Disable smooth motion");
    CHECK(loc.Translate("[\x95] Pausa ao perder o foco") == "[\x95] Pause on focus loss");
    CHECK(loc.Translate("[ ] Desativa controles de toque") == "[ ] Disable touch controls");   // E20
    CHECK(loc.HasTranslation("[<]"));
    CHECK(loc.HasTranslation("70%"));
    CHECK(loc.Translate("Carregando...\n") == "Loading...\n");   // the trailing break kept
    CHECK(loc.Translate("Configura\xE7\xF5"
                        "es") == "Settings");

    // menu.as's heredocs, as the compiler trimmed them - with the file's own
    // line endings, without CRs, and as game/script/Script.hpp holds them
    // (CRLF, ending in the lone CR AngelScript 2.20.0 left): the lookup
    // normalises all three to one key.
    const std::string menu = ReadBytes(kApp + "/menu.as");
    for (const char* name : {"como_jogar", "config", "creditos", "novo_jogo"}) {
        const std::string text = Heredoc(menu, name);
        CHECK_MSG(!text.empty(), std::string("menu.as heredoc ") + name);
        CheckTranslated(loc, text, name);
        CheckTranslated(loc, WithoutCr(text), name);
        std::string crlf;
        for (const char c : WithoutCr(text)) {
            if (c == '\n') crlf += '\r';
            crlf += c;
        }
        CheckTranslated(loc, crlf + "\r", name);
        CHECK(loc.Translate(text) != text);
    }
    // The bullet (0x95, cp1252 not Latin-1) survives into the English.
    CHECK(loc.Translate(Heredoc(menu, "como_jogar")).rfind("\x95"
                                                           "Controls",
                                                           0) == 0);

    // data.enml: the lore signs and the arena texts.
    const std::string enml = WithoutCr(ReadBytes(kApp + "/data.enml"));
    int enmlTexts = 0;
    for (const char* key : {"story01", "story02", "story03", "story04", "soldados", "fun", "comboTip", "bridge",
                            "annoying", "flyingWall", "portalToCastle", "warning", "nights", "wisdom", "arena1",
                            "arena2", "arena3", "arena4", "arena5", "arena6"}) {
        const std::string value = EnmlValue(enml, key);
        CHECK_MSG(!value.empty(), std::string("data.enml ") + key);
        if (value.empty()) continue;
        ++enmlTexts;
        CheckTranslated(loc, value, std::string("data.enml ") + key);
        CHECK(loc.Translate(value) != value);
        // The line structure is kept: as many lines in English.
        const std::string english = loc.Translate(value);
        CHECK_EQ(std::count(english.begin(), english.end(), '\n'), std::count(value.begin(), value.end(), '\n'));
    }
    CHECK_EQ(enmlTexts, 20);

    // The scenes: help signs' messages and the arena thumbnails' titles.
    int messages = 0;
    int titles = 0;
    for (const auto& entry : std::filesystem::directory_iterator(kApp + "/scenes")) {
        if (entry.path().extension() != ".esc") continue;
        const std::string scene = ReadBytes(entry.path().string());
        for (const std::string& text : SceneStrings(scene, "message")) {
            ++messages;
            CheckTranslated(loc, text, entry.path().filename().string());
        }
        for (const std::string& text : SceneStrings(scene, "title")) {
            ++titles;
            CheckTranslated(loc, text, entry.path().filename().string());
        }
    }
    CHECK_EQ(messages, 16);
    CHECK_EQ(titles, 6);
    CHECK(loc.Translate("Golpe de espada: tecla 'S'") == "Sword strike: 'S' key");

    // Composed strings, built as the scripts build them.
    CHECK(loc.Translate("\n\nJogador 2 \xE9 o vencedor!\n") == "\n\nPlayer 2 wins!\n");
    CHECK(loc.Translate("\xC9 necess\xE1rio ter 25 mana para este combo") == "You need 25 mana for this combo");
    CHECK(loc.Translate("\xC9 necess\xE1rio ter 50 mana para esta magia") == "You need 50 mana for this spell");
    CHECK(loc.Translate("[\x95] Janela") == "[\x95] Windowed");
    CHECK(loc.Translate("[ ] Tela-cheia") == "[ ] Fullscreen");
    CHECK(loc.Translate("hp: 75") == "hp: 75");
    CHECK(loc.HasTranslation("1024x768x32"));
    CHECK(loc.HasTranslation("12:05"));   // no letters: its own translation
    const std::string times = "1    1:05\n2    2:10\n3    0:00\n4    0:00\n5    0:00\n";
    CHECK(loc.Translate("Melhores tempos:\n" + times + "\n\n") == "Best times:\n" + times + "\n\n");
    const std::string versus = std::string("Escolha uma arena e dispute uma\n") + "partida contra outro jogador.\n\n" +
                               "-Quem derrotar o outro ganha 1 ponto\n" + "-Vence quem fizer " + "3" +
                               " pontos primeiro\n" + "-A partida acaba se a diferen\xE7" + "a no\n" +
                               "placar exceder " + "3" + " pontos";
    CheckTranslated(loc, versus, "menu.as versus");
    CHECK(loc.Translate(versus).find("-The first to score 3 points wins") != std::string::npos);
    const std::string noJoystick = std::string("\xC9 necess\xE1rio ao menos um joystick\n para jogar neste modo.") +
                                   "\n" + "\n" + "J\xE1 h\xE1 um joystick plugado." + "\n" +
                                   "Mude as op\xE7\xF5" + "es de entrada no menu" + "\n" +
                                   "de configura\xE7\xF5" + "es para poder" + "\n" +
                                   "utilizar o teclado e o joystick" + "\n" + "por 2 jogadores.";
    CheckTranslated(loc, noJoystick, "menu.as versus without a joystick");
    // An arena's blurb with the locked-arena suffix (menu.as:323).
    const std::string arena = EnmlValue(enml, "arena5") + "\n\n\xC9 necess\xE1rio terminar o jogo\nem menos de " +
                              "12:00" + " para\nliberar esta arena.";
    CheckTranslated(loc, arena, "arena5 locked");
    CHECK(loc.Translate(arena).find("You must finish the game\nin under 12:00 to\nunlock this arena.") !=
          std::string::npos);

    // Portuguese is the original, untouched; an unknown text stays as it was.
    CHECK(loc.Translate(versus, Language::Portuguese) == versus);
    CHECK(!loc.HasTranslation("Uma frase que n\xE3o existe"));
    CHECK(loc.Translate("Uma frase que n\xE3o existe") == "Uma frase que n\xE3o existe");

    // The pattern rules on a document of their own: a reordered capture, a
    // {text} capture translated in turn.
    Render::Localization own;
    std::string error;
    CHECK(own.LoadFromJson(
        "{\"strings\": {\"Ol\xC3\xA1\": \"Hello\"},"
        " \"patterns\": [{\"pt\": \"{text} ({int})\", \"en\": \"{2}: {1}\"}]}",
        error));
    CHECK(own.Translate("Ol\xE1 (7)") == "7: Hello");
    CHECK(own.Translate("Ol\xE1") == "Hello");
    CHECK(!own.HasTranslation("Tchau (7)"));
}

// ENHANCEMENT E16: the control hints' touch wording (strings.json "touch").
// A text that holds one of these names a key or a button to press (cp1252).
bool NamesAControl(const std::string& text) {
    for (const char* word : {"seta", "Seta", "tecla", "Tecla", "CTRL", "espa\xE7o", "ESPA\xC7O", "joystick",
                             "para cima", "segure", "Pressione", "Enter", "'S'", "'D'", "START"}) {
        if (text.find(word) != std::string::npos) return true;
    }
    return false;
}

// What a touch wording must no longer say: the keyboard's keys. The
// joystick, only in a one-line hint: the how-to-play keeps the 2-player
// part, whose second pad is still a pad.
bool NamesAKey(const std::string& text) {
    for (const char* word : {"CTRL", "tecla", "espa\xE7o", "ESPA\xC7O", "'S'", "'D'", "SPACE", "space", " key",
                             "segure J", "hold J", "setas ou", "arrow keys", "d-pad", "< e >", "< and >"}) {
        if (text.find(word) != std::string::npos) return true;
    }
    return text.find('\n') == std::string::npos && text.find("joystick") != std::string::npos;
}

// The form strings.json keys take: line ends normalised, trimmed.
std::string Key(const std::string& cp1252) {
    const std::string text = Render::Localization::Normalise(cp1252);
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\n' || text[begin] == '\t')) ++begin;
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\n' || text[end - 1] == '\t')) --end;
    return text.substr(begin, end - begin);
}

// What HudRenderer turns into quads: one for each character with pixels.
std::size_t Inked(const std::string& text) {
    return static_cast<std::size_t>(
        std::count_if(text.begin(), text.end(), [](char c) { return c != ' ' && c != '\n' && c != '\t'; }));
}

void TestTouchHints() {
    enum class Where { Literal, Label, Heredoc, Enml, Sign, Title, Composed };
    struct Text {
        std::string text;
        Where kind;
        std::string where;
    };
    // Every text of the original's that reaches the HUD, as the suite above
    // collects them, and the enhanced rows' labels.
    std::vector<Text> texts;
    for (const Literal& literal : kLiterals) texts.push_back({Unescape(literal.source), Where::Literal, literal.file});
    for (const char* label : kE10Labels) texts.push_back({label, Where::Label, "videoModes.cpp (E10)"});
    const std::string menu = ReadBytes(kApp + "/menu.as");
    for (const char* name : {"como_jogar", "config", "creditos", "novo_jogo"}) {
        texts.push_back({Heredoc(menu, name), Where::Heredoc, std::string("menu.as ") + name});
    }
    const std::string enml = WithoutCr(ReadBytes(kApp + "/data.enml"));
    for (const char* key : {"story01", "story02", "story03", "story04", "soldados", "fun", "comboTip", "bridge",
                            "annoying", "flyingWall", "portalToCastle", "warning", "nights", "wisdom", "arena1",
                            "arena2", "arena3", "arena4", "arena5", "arena6"}) {
        texts.push_back({EnmlValue(enml, key), Where::Enml, std::string("data.enml ") + key});
    }
    for (const auto& entry : std::filesystem::directory_iterator(kApp + "/scenes")) {
        if (entry.path().extension() != ".esc") continue;
        const std::string scene = ReadBytes(entry.path().string());
        for (const std::string& text : SceneStrings(scene, "message")) {
            texts.push_back({text, Where::Sign, entry.path().filename().string()});
        }
        for (const std::string& text : SceneStrings(scene, "title")) {
            texts.push_back({text, Where::Title, entry.path().filename().string()});
        }
    }
    const std::string noJoystick = std::string("\xC9 necess\xE1rio ao menos um joystick\n para jogar neste modo.") +
                                   "\n\nJ\xE1 h\xE1 um joystick plugado.\nMude as op\xE7\xF5" +
                                   "es de entrada no menu\nde configura\xE7\xF5" +
                                   "es para poder\nutilizar o teclado e o joystick\npor 2 jogadores.";
    texts.push_back({noJoystick, Where::Composed, "menu.as versus without a joystick"});

    // The texts that name a control and keep the original's wording, each for
    // a reason.
    const std::pair<std::string, const char*> kept[] = {
        {"Pressione Alt+Enter para trocar entre fullscreen e modo janela",
         "E20 does not draw it on a phone; a desktop with touch keeps the keyboard it names"},
        // E22: the first paragraph alone has its touch wording (a pad for
        // player 2); both paragraphs are drawn only with a pad on player 1's
        // index, which the touch controls never give a pad.
        {noJoystick, "never drawn while the touch controls are on (E22)"},
        {Heredoc(menu, "config"), "the settings blurb: what the screen sets up"},
        {"2\xBA joystick para jogador 2", "a row's label"},
        {"1\xBA joystick para jogador 1", "a row's label"},
        {"Teclado para o jogador 2", "a row's label"},
        {"Jogador 2 s\xF3 no joystick", "a row's label"},
    };

    Render::Localization plain;   // never told about touch: today's text
    CHECK(plain.Load());
    Render::Localization loc;
    CHECK(loc.Load());
    CHECK(!loc.Touch());

    // Every text that names a control has its touch wording in both
    // languages, unless it is kept, and no other text has one.
    std::set<std::string> worded;
    std::vector<const Text*> hints;
    for (const Text& t : texts) {
        CHECK_MSG(!t.text.empty(), t.where);
        const bool names = NamesAControl(t.text);
        const bool keep = std::any_of(std::begin(kept), std::end(kept),
                                      [&](const auto& k) { return Key(k.first) == Key(t.text); });
        const bool touch = loc.HasTouchVariant(t.text);
        if (names && !keep) CHECK_MSG(touch, t.where + ": names a control but has no touch wording: " + t.text);
        if (!names || keep) CHECK_MSG(!touch, t.where + ": has touch wording it should not: " + t.text);
        if (touch && worded.insert(Key(t.text)).second) hints.push_back(&t);
    }
    // Every "touch" key is one of these texts (a typo would never match).
    CHECK_EQ(worded.size(), loc.TouchCount());
    // Seven help signs, the combo lore sign, the how-to-play panel, and
    // Versus without a second controller (E22).
    CHECK_EQ(loc.TouchCount(), std::size_t{10});

    Render::FontAtlas fonts;
    fonts.SetSystemFontsEnabled(false);   // the stand-ins: the same widths on every machine
    for (const Text* hint : hints) {
        const std::string& text = hint->text;
        const std::string ptToday = plain.Translate(text, Language::Portuguese);
        const std::string enToday = plain.Translate(text, Language::English);
        CHECK(ptToday == text);
        // Off: exactly today's, in both languages.
        CHECK_MSG(loc.Translate(text, Language::Portuguese) == ptToday, hint->where);
        CHECK_MSG(loc.Translate(text, Language::English) == enToday, hint->where);

        // On: the touch wording in either language, no key named.
        loc.SetTouch(true);
        const std::string pt = loc.Translate(text, Language::Portuguese);
        const std::string en = loc.Translate(text, Language::English);
        CHECK_MSG(pt != ptToday && en != enToday, hint->where);
        CHECK_MSG(!NamesAKey(pt), hint->where + ": " + pt);
        CHECK_MSG(!NamesAKey(en), hint->where + ": " + en);
        // The language switched with touch on, and back.
        CHECK(loc.Translate(text, Language::Portuguese) == pt);
        CHECK(loc.Translate(text, Language::English) == en);
        // Its blank lines around it kept, as a translation keeps them.
        CHECK(loc.Translate("\n" + text + "\n", Language::English) == "\n" + en + "\n");
        // What the Language setting draws.
        loc.SetLanguage(Language::Portuguese);
        CHECK(loc.Translate(text) == pt);
        loc.SetLanguage(Language::English);
        CHECK(loc.Translate(text) == en);

        // Where it is drawn, it fits.
        for (const std::string& wording : {pt, en}) {
            if (hint->kind == Where::Sign) {
                // addMessage's line at (10,70) in Arial 30, on the narrowest screen.
                const float width = fonts.Layout(wording, "Arial", 30.0f, glm::vec2(10.0f, 70.0f)).width;
                CHECK_MSG(width > 0.0f && 10.0f + width <= 1024.0f, wording + ": " + std::to_string(width));
            } else if (hint->kind == Where::Enml) {
                // A lore sign: as many lines as the original's.
                CHECK_EQ(std::count(wording.begin(), wording.end(), '\n'), std::count(text.begin(), text.end(), '\n'));
            } else if (hint->kind == Where::Heredoc || (hint->kind == Where::Literal && hint->where == "menu.as")) {
                // (menu.as's one worded literal is Versus without a second
                // controller, E22, drawn by showData as the heredocs are.)
                // showData's panel: 391 px from x 633, the text at +10; from y 70
                // in Arial Narrow 25 on the 768 px menu (menu.as:217-229).
                const Render::TextLayout layout = fonts.Layout(wording, "Arial Narrow", 25.0f, glm::vec2(0.0f));
                CHECK_MSG(layout.width > 0.0f && layout.width <= 381.0f, std::to_string(layout.width));
                CHECK_MSG(70.0f + static_cast<float>(layout.lines) * layout.lineHeight <= 768.0f,
                          std::to_string(layout.lines) + " lines");
            }
        }

        // Off again: today's text, byte for byte.
        loc.SetTouch(false);
        CHECK_MSG(loc.Translate(text, Language::Portuguese) == ptToday, hint->where);
        CHECK_MSG(loc.Translate(text, Language::English) == enToday, hint->where);
    }

    // The how-to-play as the compiler read it, without CRs, and as
    // Script.hpp holds it (CRLF, a lone CR at the end, which is a line break
    // and stays one): one wording.
    const std::string howTo = Heredoc(menu, "como_jogar");
    std::string crlf;
    for (const char c : WithoutCr(howTo)) {
        if (c == '\n') crlf += '\r';
        crlf += c;
    }
    loc.SetTouch(true);
    const std::string howToEn = loc.Translate(howTo, Language::English);
    CHECK(howToEn.rfind("\x95"
                        "Touch controls",
                        0) == 0);
    CHECK(loc.Translate(WithoutCr(howTo), Language::English) == howToEn);
    CHECK(loc.Translate(crlf + "\r", Language::English) == howToEn + "\n");
    CHECK(plain.Translate(crlf + "\r", Language::English) == plain.Translate(howTo, Language::English) + "\n");
    CHECK(loc.Translate(howTo, Language::Portuguese).rfind("\x95"
                                                           "Controles de toque",
                                                           0) == 0);
    // The 2-player part is the original's, word for word.
    CHECK(howToEn.find(" press START on the 2nd controller.") != std::string::npos);
    CHECK(loc.Translate("Utilize as setas ou as direcionais do joystick para mover-se", Language::Portuguese) ==
          "Utilize as setas no canto inferior esquerdo para mover-se");
    CHECK(loc.Translate("Golpe de espada: tecla 'S'", Language::English) == "Sword strike: the sword button");
    // A text touch does not list goes its usual way.
    CHECK(loc.Translate("Checkpoint...", Language::English) == "Checkpoint...");
    CHECK(loc.Translate("Caveiras recuperam seu HP", Language::English) == "Skulls restore your HP");
    CHECK(loc.Translate("Caveiras recuperam seu HP", Language::Portuguese) == "Caveiras recuperam seu HP");

    // HudRenderer draws the touch wording while touch is on, and today's
    // text once it is off again: a quad for each inked character.
    entt::registry registry;
    Render::TextureCache textures(kApp);
    Render::FontAtlas hudFonts;
    hudFonts.SetSystemFontsEnabled(false);
    Render::HudRenderer hud;
    hud.Attach(registry, textures, hudFonts, loc);
    Render::View view;
    view.logicalScreen = glm::vec2(1024.0f, 768.0f);
    view.windowPixels = glm::uvec2(1024, 768);
    view.scale = 1.0f;
    view.viewportMin = glm::vec2(0.0f);
    view.viewportMax = glm::vec2(1024.0f, 768.0f);
    Eth::RenderSnapshot snapshot;
    Eth::HudCmd sign;
    sign.kind = Eth::HudCmd::Kind::Text;
    sign.text = "Utilize as setas ou as direcionais do joystick para mover-se";
    sign.font = "Arial";
    sign.fontSize = 30.0f;
    sign.pos = glm::vec2(10.0f, 70.0f);
    sign.color = 0xFFCBCBE4u;
    snapshot.hud.push_back(sign);
    const auto quadsFor = [&]() {
        std::vector<Supersonic::ScreenOverlay::Quad> quads;
        hud.Build(snapshot, view, quads);
        return quads.size();
    };
    loc.SetLanguage(Language::English);
    loc.SetTouch(true);
    CHECK_EQ(quadsFor(), Inked("Use the arrows at the bottom left to move"));
    loc.SetTouch(false);
    CHECK_EQ(quadsFor(), Inked("Use the arrow keys or the joystick's d-pad to move"));
    loc.SetLanguage(Language::Portuguese);
    CHECK_EQ(quadsFor(), Inked(sign.text));
    loc.SetTouch(true);
    CHECK_EQ(quadsFor(), Inked("Utilize as setas no canto inferior esquerdo para mover-se"));
    hud.Detach();
}

bool EndsWithNoCase(std::string text, std::string tail) {
    const auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](char c) {
            return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
        });
        return s;
    };
    text = lower(text);
    tail = lower(tail);
    return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
}

// The bundled stand-ins (game/data/fonts, FontAtlas.hpp): there for every face
// on every machine, and - where the Windows face is installed to measure them
// against - laid out as that face lays out.
void TestFontStandIns() {
    Render::FontAtlas standIns;
    standIns.SetSystemFontsEnabled(false);
    const struct {
        const char* face;
        const char* file;
        const char* windowsFile;
        float tolerancePx;   // how far a line may drift from the Windows face's
    } kFaces[] = {
        {"Arial Narrow", "LiberationSans-Bold.ttf", "ARIALNB.TTF", 2.0f},   // advances x 0.82: 1/2048 em apart
        {"Arial", "LiberationSans-Bold.ttf", "arialbd.ttf", 1.0f},          // metric-compatible
        {"Arial Black", "DejaVuSans-Bold.ttf", "ariblk.ttf", -1.0f},        // close, not compatible: not measured
        {"Verdana", "DejaVuSans-Bold.ttf", "verdanab.ttf", -1.0f},
    };
    Render::FontAtlas system;   // the system's faces first, where there are any
    const char* const kLines[] = {"hp: 100", "Carregando...", "Op\xE7\xF5" "es de v\xED" "deo", "12:05",
                                  "Pressione Alt+Enter para trocar entre fullscreen e modo janela"};
    int measured = 0;
    for (const auto& f : kFaces) {
        CHECK_MSG(EndsWithNoCase(standIns.FaceFile(f.face), f.file), f.face);
        CHECK_MSG(standIns.FaceIsStandIn(f.face), f.face);
        CHECK_MSG(!system.FaceFile(f.face).empty(), f.face);   // a stand-in at worst
        if (f.tolerancePx < 0.0f || !EndsWithNoCase(system.FaceFile(f.face), f.windowsFile)) continue;
        ++measured;
        CHECK(!system.FaceIsStandIn(f.face));
        for (const float size : {16.0f, 30.0f, 40.0f}) {
            for (const char* line : kLines) {
                const Render::TextLayout ours = standIns.Layout(line, f.face, size, glm::vec2(0.0f));
                const Render::TextLayout theirs = system.Layout(line, f.face, size, glm::vec2(0.0f));
                // The same cell, so the same line height and baseline, exactly.
                CHECK_NEAR(ours.lineHeight, theirs.lineHeight);
                CHECK_NEAR(ours.ascent, theirs.ascent);
                CHECK_MSG(std::fabs(ours.width - theirs.width) <= f.tolerancePx,
                          std::string(f.face) + " " + std::to_string(size) + " \"" + line + "\": " +
                              std::to_string(ours.width) + " vs " + std::to_string(theirs.width));
            }
        }
    }
    if (measured == 0) {
        std::printf("  (the stand-ins are not measured: no Windows faces in \"%s\")\n",
                    Render::FontAtlas::FontsDirectory().c_str());
    }
    // Two stand-ins from one file are two fonts: the narrow one is narrower.
    const float narrowWidth = standIns.Layout("Carregando...", "Arial Narrow", 30.0f, glm::vec2(0.0f)).width;
    const float arialWidth = standIns.Layout("Carregando...", "Arial", 30.0f, glm::vec2(0.0f)).width;
    CHECK(narrowWidth > 0.7f * arialWidth && narrowWidth < 0.9f * arialWidth);
}

void TestFontAtlas() {
    Render::FontAtlas fonts;
    // Arial Narrow Bold itself on Windows; the stand-in anywhere else.
    const std::string narrow = fonts.FaceFile("Arial Narrow");
    if (narrow.empty()) {
        std::printf("  (FontAtlas layout skipped: no Arial Narrow Bold in %s and no stand-in in %s)\n",
                    Render::FontAtlas::FontsDirectory().c_str(), fonts.BundledFontsDirectory().c_str());
        return;
    }
    fonts.SetRasterScale(1.0f);

    // "Olá\nmundo": eight glyphs with pixels, two lines, anchored at the
    // truncated position.
    const Render::TextLayout layout = fonts.Layout("Ol\xE1\nmundo", "Arial Narrow", 25.0f, glm::vec2(10.7f, 20.2f));
    CHECK(layout.texture.rfind("penumbra:font:", 0) == 0);
    CHECK_EQ(layout.glyphs.size(), std::size_t{8});
    CHECK_EQ(layout.lines, 2);
    // GDI's tmHeight for a 25-pixel cell: the cell itself, give or take the
    // rounding of ascent and descent.
    CHECK(layout.lineHeight >= 24.0f && layout.lineHeight <= 26.0f);
    if (layout.glyphs.size() == 8) {
        const Render::TextGlyph& first = layout.glyphs[0];
        const Render::TextGlyph& accent = layout.glyphs[2];
        const Render::TextGlyph& second = layout.glyphs[3];
        CHECK_EQ(first.byte, static_cast<unsigned char>('O'));
        CHECK_EQ(accent.byte, static_cast<unsigned char>(0xE1));
        CHECK_EQ(second.line, 1);
        CHECK_NEAR(first.pen.x, 10.0f);
        CHECK_NEAR(second.pen.x, 10.0f);
        CHECK_NEAR(first.pen.y, 20.0f + layout.ascent);
        CHECK_NEAR(second.pen.y - first.pen.y, layout.lineHeight);
        // Pixel-aligned at scale 1.
        CHECK_NEAR(first.min.x, std::round(first.min.x));
        CHECK_NEAR(first.min.y, std::round(first.min.y));
    }

    // 0xE1 is U+00E1: as wide as an 'a' and taller (the acute accent).
    const Render::TextLayout plain = fonts.Layout("ax", "Arial Narrow", 25.0f, glm::vec2(0.0f));
    const Render::TextLayout acute = fonts.Layout("\xE1x", "Arial Narrow", 25.0f, glm::vec2(0.0f));
    CHECK_EQ(plain.glyphs.size(), std::size_t{2});
    CHECK_EQ(acute.glyphs.size(), std::size_t{2});
    if (plain.glyphs.size() == 2 && acute.glyphs.size() == 2) {
        CHECK_NEAR(plain.glyphs[1].pen.x, acute.glyphs[1].pen.x);
        CHECK(acute.glyphs[0].max.y - acute.glyphs[0].min.y > plain.glyphs[0].max.y - plain.glyphs[0].min.y);
        CHECK_NEAR(acute.glyphs[0].max.y, plain.glyphs[0].max.y);   // same baseline, same foot
    }

    // CR LF is one break; tabs go to stops of a fixed width.
    const Render::TextLayout crlf = fonts.Layout("Ol\xE1\r\nmundo", "Arial Narrow", 25.0f, glm::vec2(10.7f, 20.2f));
    CHECK_EQ(crlf.lines, 2);
    CHECK_EQ(crlf.glyphs.size(), std::size_t{8});
    const Render::TextLayout oneTab = fonts.Layout("\tb", "Arial Narrow", 25.0f, glm::vec2(0.0f));
    const Render::TextLayout twoTabs = fonts.Layout("\t\tb", "Arial Narrow", 25.0f, glm::vec2(0.0f));
    if (oneTab.glyphs.size() == 1 && twoTabs.glyphs.size() == 1) {
        CHECK(oneTab.glyphs[0].pen.x > 0.0f);
        CHECK_NEAR(twoTabs.glyphs[0].pen.x, 2.0f * oneTab.glyphs[0].pen.x);
    }

    // ENHANCED: rasterised at twice the pixels, laid out in the same place.
    fonts.SetRasterScale(2.0f);
    const Render::TextLayout sharp = fonts.Layout("Ol\xE1\nmundo", "Arial Narrow", 25.0f, glm::vec2(10.7f, 20.2f));
    CHECK_EQ(sharp.glyphs.size(), std::size_t{8});
    CHECK(sharp.lineHeight >= 24.0f && sharp.lineHeight <= 26.0f);
    if (sharp.glyphs.size() == 8) {
        CHECK_NEAR(sharp.glyphs[0].pen.x, 10.0f);
        // Every edge on a whole window pixel.
        CHECK_NEAR(sharp.glyphs[4].min.x * 2.0f, std::round(sharp.glyphs[4].min.x * 2.0f));
    }
    CHECK(sharp.texture != layout.texture);

    // The 256-pixel clock bakes only what it draws.
    fonts.SetRasterScale(1.0f);
    const Render::TextLayout clock = fonts.Layout("12:05", "Arial Narrow", 256.0f, glm::vec2(100.0f, 100.0f));
    CHECK_EQ(clock.glyphs.size(), std::size_t{5});
    CHECK(clock.lineHeight >= 250.0f && clock.lineHeight <= 262.0f);

    // Every face the scripts name resolves to something, on any machine.
    for (const char* face : {"Arial Black", "Arial", "Verdana"}) {
        CHECK_MSG(!fonts.FaceFile(face).empty(), face);
    }
}

// ENHANCEMENT E21: the enhanced edition's credit after the original team's,
// and the one byte beyond cp1252 it needs (0x8D, the port's U+0107).
void TestEnhancedCredits() {
    // The panel's text as menu.cpp draws it (menu.as:252 creditos, then E21's).
    const std::string credits = Script::creditos + Script::creditosEnhanced;
    CHECK(credits.compare(0, Script::creditos.size(), Script::creditos) == 0);   // the original's, untouched
    CHECK(credits.find("\r\n\r\nIvan Cvetanovi\x8D\r\n -Edi\xE7\xE3o aprimorada (Supersonic Engine)") !=
          std::string::npos);
    // One blank line between the teams: the original's lone CR at its end and
    // the LF that follows it are one break.
    CHECK(Render::Localization::Normalise(credits).find("-Taina Monclaire\n\nIvan Cvetanovi\x8D\n") !=
          std::string::npos);

    Render::Localization loc;
    CHECK(loc.Load());
    // Portuguese: the bytes as they are, 0x8D included; normalising keeps it.
    CHECK(loc.Translate(credits, Language::Portuguese) == credits);
    CHECK(Render::Localization::Normalise("Cvetanovi\x8D\r\n") == "Cvetanovi\x8D\n");
    // English: the original's credits whole, then the name - its 0x8D read
    // from strings.json's UTF-8 U+0107 - and the role.
    CheckTranslated(loc, credits, "E21 credits");
    const std::string english = loc.Translate(credits, Language::English);
    const std::string originalEnglish = loc.Translate(Script::creditos, Language::English);
    CHECK(english.rfind("Andr\xE9 Santee\n -Programming", 0) == 0);
    CHECK(english.compare(0, originalEnglish.size() - 1, originalEnglish, 0, originalEnglish.size() - 1) == 0);
    CHECK(english.find("-Taina Monclaire\n\nIvan Cvetanovi\x8D\n -Enhanced edition (Supersonic Engine)") !=
          std::string::npos);
    const std::string role = "\n -Enhanced edition (Supersonic Engine)";
    CHECK(english.size() > role.size() && english.compare(english.size() - role.size(), role.size(), role) == 0);
    // The original's heredoc alone keeps its own translation, without E21's.
    CHECK(originalEnglish.find("Ivan") == std::string::npos);

    // It fits showData's panel in both languages, with the Windows faces
    // where this machine has them and with the stand-ins: 381 px from x 643,
    // from y 70 in Arial Narrow 25 on the 768 px menu (menu.as:217-229).
    for (const bool system : {true, false}) {
        Render::FontAtlas fonts;
        fonts.SetSystemFontsEnabled(system);
        if (fonts.FaceFile("Arial Narrow").empty()) continue;
        for (const std::string& text : {credits, english}) {
            const Render::TextLayout layout = fonts.Layout(text, "Arial Narrow", 25.0f, glm::vec2(0.0f));
            std::printf("  E21 credits (%s, %s): %d lines, %.0f px wide, bottom at y %.0f\n",
                        system ? "system faces" : "stand-ins", text == english ? "en" : "pt", layout.lines,
                        layout.width, 70.0f + static_cast<float>(layout.lines) * layout.lineHeight);
            CHECK_EQ(layout.lines, 27);   // the original's 24, a blank line, the name, the role
            CHECK_MSG(layout.width > 0.0f && layout.width <= 381.0f, std::to_string(layout.width));
            CHECK_MSG(70.0f + static_cast<float>(layout.lines) * layout.lineHeight <= 768.0f,
                      std::to_string(layout.lines) + " lines");
        }
    }

    // THE GLYPH: every face the scripts name draws 0x8D with its file's own
    // U+0107, in both font sets - the Windows files where this machine has
    // them (ARIALNB, arialbd, ariblk, verdanab) and the bundled stand-ins
    // (Liberation Sans Bold, DejaVu Sans Bold) - and not with 'c' or .notdef.
    int systemFaces = 0;
    for (const bool system : {true, false}) {
        Render::FontAtlas fonts;
        fonts.SetSystemFontsEnabled(system);
        for (const char* face : {"Arial Narrow", "Arial", "Arial Black", "Verdana"}) {
            const std::string file = fonts.FaceFile(face);
            CHECK_MSG(!file.empty(), face);
            if (file.empty()) continue;
            if (system && !fonts.FaceIsStandIn(face)) ++systemFaces;
            const int cAcute = fonts.GlyphForCodePoint(face, 0x0107u);
            std::printf("  %s -> %s: 0x8D is glyph %d, U+0107 glyph %d\n", face, file.c_str(),
                        fonts.GlyphForByte(face, 0x8D), cAcute);
            CHECK_MSG(cAcute != 0, file + " has no U+0107");
            CHECK_MSG(fonts.GlyphForByte(face, Eth::kCAcuteByte) == cAcute, file);
            CHECK_MSG(fonts.GlyphForByte(face, 'c') != cAcute, file);
            CHECK_EQ(fonts.GlyphForByte(face, 0xE7), fonts.GlyphForCodePoint(face, 0xE7u));   // the rest as before
        }
        // Drawn: as wide as a 'c' (the pen after it is where it is after 'c'),
        // taller (the acute), on the same foot.
        const Render::TextLayout plain = fonts.Layout("cx", "Arial Narrow", 25.0f, glm::vec2(0.0f));
        const Render::TextLayout acute = fonts.Layout("\x8Dx", "Arial Narrow", 25.0f, glm::vec2(0.0f));
        CHECK_EQ(acute.glyphs.size(), std::size_t{2});
        if (plain.glyphs.size() == 2 && acute.glyphs.size() == 2) {
            CHECK_EQ(acute.glyphs[0].byte, static_cast<unsigned char>(0x8D));
            CHECK_NEAR(plain.glyphs[1].pen.x, acute.glyphs[1].pen.x);
            CHECK(acute.glyphs[0].max.y - acute.glyphs[0].min.y > plain.glyphs[0].max.y - plain.glyphs[0].min.y + 3.0f);
            CHECK_NEAR(acute.glyphs[0].max.y, plain.glyphs[0].max.y);
        }
    }
    if (systemFaces == 0) {
        std::printf("  (no Windows faces in \"%s\": the system set is the stand-ins here)\n",
                    Render::FontAtlas::FontsDirectory().c_str());
    }
}

void TestHudRenderer() {
    entt::registry registry;
    Render::TextureCache textures(kApp);
    Render::FontAtlas fonts;
    Render::Localization loc;
    loc.Load();
    Render::HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);

    // A 4:3 screen pillarboxed in a 1920x1080 window.
    Render::View view;
    view.logicalScreen = glm::vec2(1024.0f, 768.0f);
    view.windowPixels = glm::uvec2(1920, 1080);
    view.scale = 1080.0f / 768.0f;
    view.viewportMin = glm::vec2(240.0f, 0.0f);
    view.viewportMax = glm::vec2(1680.0f, 1080.0f);

    Eth::RenderSnapshot snapshot;
    Eth::HudCmd rect;
    rect.kind = Eth::HudCmd::Kind::Rectangle;
    rect.pos = glm::vec2(0.0f);
    rect.size = glm::vec2(1024.0f, 768.0f);
    rect.color = rect.color1 = rect.color2 = rect.color3 = 0x80000000u;
    snapshot.hud.push_back(rect);

    std::vector<Supersonic::ScreenOverlay::Quad> quads;
    hud.Build(snapshot, view, quads);
    // One plain quad over the 4:3 box, then the two side bars.
    CHECK_EQ(quads.size(), std::size_t{3});
    if (quads.size() == 3) {
        CHECK_NEAR(quads[0].min.x, 240.0f / 1920.0f);
        CHECK_NEAR(quads[0].max.x, 1680.0f / 1920.0f);
        CHECK_NEAR(quads[0].color.a, 128.0f / 255.0f);
        CHECK(quads[0].texture.empty());
        CHECK_NEAR(quads[1].max.x, 240.0f / 1920.0f);
        CHECK_NEAR(quads[2].min.x, 1680.0f / 1920.0f);
        CHECK_NEAR(quads[2].color.a, 1.0f);
    }

    // showData's panel: a vertical gradient. No TextureRegistry on a bare
    // registry, so strips, darkest at the top.
    snapshot.hud.clear();
    rect.color = rect.color1 = 0xBE000000u;
    rect.color2 = rect.color3 = 0x37000000u;
    snapshot.hud.push_back(rect);
    quads.clear();
    hud.Build(snapshot, view, quads);
    CHECK(quads.size() > 3);
    if (quads.size() > 3) {
        CHECK(quads.front().color.a > quads[quads.size() - 3].color.a);
        CHECK(quads.front().color.a < 190.0f / 255.0f + 1e-4f);
        CHECK(quads[quads.size() - 3].color.a > 55.0f / 255.0f - 1e-4f);
    }

    // Idempotent: the same snapshot builds the same quads.
    std::vector<Supersonic::ScreenOverlay::Quad> again;
    hud.Build(snapshot, view, again);
    CHECK_EQ(again.size(), quads.size());

    CHECK_NEAR(Render::HudRenderer::ToColor(0x80FF4000u).r, 1.0f);
    CHECK_NEAR(Render::HudRenderer::ToColor(0x80FF4000u).g, 64.0f / 255.0f);
    CHECK_NEAR(Render::HudRenderer::ToColor(0x80FF4000u).a, 128.0f / 255.0f);

    // Text: one quad per visible glyph, translated first ("Checkpoint..." is
    // the same in English: thirteen glyphs with pixels), then the two bars.
    if (!fonts.FaceFile("Arial").empty()) {
        snapshot.hud.clear();
        Eth::HudCmd text;
        text.kind = Eth::HudCmd::Kind::Text;
        text.text = "Checkpoint...";
        text.font = "Arial";
        text.fontSize = 30.0f;
        text.pos = glm::vec2(10.0f, 70.0f);
        text.color = 0xFFCBCBE4u;
        snapshot.hud.push_back(text);
        quads.clear();
        hud.Build(snapshot, view, quads);
        CHECK_EQ(quads.size(), std::size_t{13 + 2});
        if (!quads.empty()) CHECK(quads.front().texture.rfind("penumbra:font:", 0) == 0);
    }
}

// Step 25 (E1's wide menus): a 1024x768 menu's view in a 1920x1080 window,
// barred (sideMargin 0) or open.
Render::View MenuView(float sideMargin, glm::uvec2 window = glm::uvec2(1920u, 1080u)) {
    Eth::RenderSnapshot snapshot;
    snapshot.screenSize = Eth::vector2(1024.0f, 768.0f);
    snapshot.sideMargin = sideMargin;
    return Render::CameraRig::ComputeView(snapshot, window, true);
}

bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

Eth::HudCmd Rect(glm::vec2 pos, glm::vec2 size, Eth::uint top, Eth::uint bottom) {
    Eth::HudCmd rect;
    rect.kind = Eth::HudCmd::Kind::Rectangle;
    rect.pos = pos;
    rect.size = size;
    rect.color = rect.color1 = top;
    rect.color2 = rect.color3 = bottom;
    return rect;
}

void TestWideMenuHud() {
    entt::registry registry;
    Render::TextureCache textures(kApp);
    Render::FontAtlas fonts;
    Render::Localization loc;
    loc.Load();
    Render::HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);
    const Render::View open = MenuView(Render::kWideMenuMargin);
    const Render::View barred = MenuView(0.0f);
    const float left = 240.0f / 1920.0f;    // the 4:3 box in the window, as fractions
    const float right = 1680.0f / 1920.0f;

    // A fade (fadeIn/fadeOut, drawRect: the whole screen, util.as:379-403)
    // covers the whole window: the box, then the left and right sides in its
    // colour. No bars.
    Eth::RenderSnapshot snapshot;
    snapshot.hud.push_back(Rect({0.0f, 0.0f}, {1024.0f, 768.0f}, 0x80000000u, 0x80000000u));
    std::vector<Supersonic::ScreenOverlay::Quad> quads;
    hud.Build(snapshot, open, quads);
    CHECK_EQ(quads.size(), std::size_t{3});
    if (quads.size() == 3) {
        CHECK(Near(quads[0].min.x, left) && Near(quads[0].max.x, right));
        CHECK(Near(quads[1].min.x, 0.0f) && Near(quads[1].max.x, left));
        CHECK(Near(quads[2].min.x, right) && Near(quads[2].max.x, 1.0f));
        for (const auto& quad : quads) {
            CHECK_NEAR(quad.color.a, 128.0f / 255.0f);
            CHECK(Near(quad.min.y, 0.0f) && Near(quad.max.y, 1.0f));
        }
    }
    // Barred, as before: the box and two black bars.
    quads.clear();
    hud.Build(snapshot, barred, quads);
    CHECK_EQ(quads.size(), std::size_t{3});
    if (quads.size() == 3) CHECK_NEAR(quads[2].color.a, 1.0f);

    // showData's panel (menu.as:217-224): against the right edge, a vertical
    // gradient, in floats as the script makes it. It goes on to the window's
    // right edge in the same gradient; nothing is added on its left.
    const glm::vec2 panelSize(1024.0f * (1.0f - 0.618f), 768.0f);
    const glm::vec2 panelPos(1024.0f - panelSize.x, 0.0f);
    snapshot.hud.clear();
    snapshot.hud.push_back(Rect(panelPos, panelSize, 0xBE000000u, 0x37000000u));
    quads.clear();
    hud.Build(snapshot, open, quads);
    // No TextureRegistry on a bare registry: 48 strips for the panel, 48 for its continuation.
    CHECK_EQ(quads.size(), std::size_t{96});
    if (quads.size() == 96) {
        const float panelLeft = (240.0f + panelPos.x * 1080.0f / 768.0f) / 1920.0f;
        for (std::size_t i = 0; i < 48; ++i) {
            CHECK(Near(quads[i].min.x, panelLeft, 1e-3f) && Near(quads[i].max.x, right, 1e-3f));
            CHECK(Near(quads[48 + i].min.x, right, 1e-3f) && Near(quads[48 + i].max.x, 1.0f));
            // Row for row, the continuation is the panel's colour.
            CHECK_NEAR(quads[48 + i].color.a, quads[i].color.a);
            CHECK(Near(quads[48 + i].min.y, quads[i].min.y) && Near(quads[48 + i].max.y, quads[i].max.y));
        }
        CHECK(quads[48].color.a > quads[95].color.a);   // darkest at the top
    }

    // A rectangle that meets no edge, or lies wholly past one, is drawn as it
    // is; so is one with no width.
    snapshot.hud.clear();
    snapshot.hud.push_back(Rect({100.0f, 100.0f}, {200.0f, 200.0f}, 0xFF102030u, 0xFF102030u));
    snapshot.hud.push_back(Rect({-300.0f, 100.0f}, {200.0f, 200.0f}, 0xFF102030u, 0xFF102030u));
    snapshot.hud.push_back(Rect({1100.0f, 100.0f}, {50.0f, 50.0f}, 0xFF102030u, 0xFF102030u));
    snapshot.hud.push_back(Rect({0.0f, 100.0f}, {0.0f, 50.0f}, 0xFF102030u, 0xFF102030u));
    quads.clear();
    hud.Build(snapshot, open, quads);
    CHECK_EQ(quads.size(), std::size_t{3});
    // One against the left edge only, a horizontal gradient: the left side
    // takes its left edge's colour.
    snapshot.hud.clear();
    Eth::HudCmd ramp = Rect({0.0f, 700.0f}, {300.0f, 68.0f}, 0xFF000000u, 0xFF000000u);
    ramp.color1 = ramp.color3 = 0xFFFFFFFFu;
    snapshot.hud.push_back(ramp);
    quads.clear();
    hud.Build(snapshot, open, quads);
    CHECK(quads.size() > 1);
    if (quads.size() > 1) {
        const auto& side = quads.back();
        CHECK(Near(side.min.x, 0.0f) && Near(side.max.x, left));
        CHECK(side.color == glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        CHECK(Near(side.min.y, 1080.0f * 700.0f / 768.0f / 1080.0f));
    }
}

// Every fixed-layout scene's backdrop, from the original's own files: rows of
// the scenes' own tiles, continued at their step, meeting the file's tiles
// with no gap and on none of them, far enough for a 4:1 window.
void TestWideMenuBackdrop() {
    struct Expect {
        const char* scene;
        std::size_t copies;
        std::set<std::string> names;
    };
    const Expect expects[] = {
        // The four full rows of white_ground (x 128..1920) go on left 4 tiles
        // each; the fifth (y 1152, x 128..1408) 4 left and 2 right, to 2048.
        {"menu.esc", 22, {"white_ground.ent"}},
        {"arena_select.esc", 16, {"white_ground.ent"}},
        // Five rows of four columns, 4 more each way.
        {"videoModes.esc", 40, {"wall11.ent", "wall10.ent", "face_no_light.ent", "arch01.ent", "floor03.ent"}},
        {"gameover.esc", 0, {}},
    };
    for (const Expect& expect : expects) {
        const std::string path = kApp + "/scenes/" + expect.scene;
        const std::optional<Eth::SceneFile> file = Eth::ReadSceneFile(path);
        CHECK_MSG(file.has_value(), path);
        if (!file) continue;
        const std::string scene = std::string("scenes/") + expect.scene;
        const Eth::SceneWidening widening = Render::WidenScene(scene, *file);
        CHECK_MSG(widening.sideMargin == Render::kWideMenuMargin, scene);
        CHECK_MSG(widening.backdrop.size() == expect.copies, scene + ": " + std::to_string(widening.backdrop.size()));
        // Each row, file and copies together: x from -896 on, a tile every
        // 256 up to at least 1920 (covering -1024..2048), none twice.
        std::map<std::pair<std::string, std::pair<float, float>>, std::vector<float>> rows;
        for (const Eth::ScenePlacement& p : file->entities) {
            if (expect.names.count(p.entityName) != 0) rows[{p.entityName, {p.position.y, p.position.z}}].push_back(p.position.x);
        }
        for (const Eth::ScenePlacement& copy : widening.backdrop) {
            CHECK_MSG(expect.names.count(copy.entityName) == 1, copy.entityName);
            CHECK_MSG(copy.position.x < 0.0f || copy.position.x > 1024.0f, scene);
            auto& row = rows[{copy.entityName, {copy.position.y, copy.position.z}}];
            CHECK_MSG(std::find(row.begin(), row.end(), copy.position.x) == row.end(), scene + ": a copy on a tile");
            row.push_back(copy.position.x);
            // As the file lays it: the same image, lighting and depth.
            const auto original = std::find_if(file->entities.begin(), file->entities.end(),
                                               [&copy](const Eth::ScenePlacement& p) { return p.entityName == copy.entityName; });
            if (original != file->entities.end()) {
                CHECK(copy.def.sprite == original->def.sprite);
                CHECK(copy.def.normal == original->def.normal);
                CHECK(copy.def.type == original->def.type);
                CHECK(copy.def.isStatic);
            }
        }
        for (auto& [key, xs] : rows) {
            std::sort(xs.begin(), xs.end());
            // The rows the screen shows (y < 768) reach the margin's end both ways.
            if (key.second.first < 768.0f) {
                CHECK_MSG(!xs.empty() && xs.front() <= -896.0f && xs.back() >= 1920.0f, scene + " " + key.first);
            }
            for (std::size_t i = 1; i < xs.size(); ++i) CHECK_MSG(xs[i] - xs[i - 1] == 256.0f, scene + " " + key.first);
        }
    }
    // Not a fixed-layout scene: nothing.
    const std::optional<Eth::SceneFile> level = Eth::ReadSceneFile(kApp + "/scenes/level1.esc");
    CHECK(level.has_value());
    if (level) {
        const Eth::SceneWidening none = Render::WidenScene("scenes/level1.esc", *level);
        CHECK(none.sideMargin == 0.0f && none.backdrop.empty());
    }
}

// The real main menu, loaded as the layer loads it with widescreen on, in a
// 16:9 window, the cursor on New Game: the world goes on past the sides, and
// everything the scripts placed is where the pillarbox put it - New Game's
// button and the panel's title at their 1024x768 places plus the 240-pixel
// margin, the panel on to the window's edge.
void TestWideMenuScene() {
    Eth::MachineConfig config;
    config.userRoot.clear();   // nothing is written
    config.widenScene = [](const std::string& scene, const Eth::SceneFile& file) {
        return Render::WidenScene(scene, file);
    };
    Eth::Machine machine(config);
    Eth::Machine::Scope scope(machine);
    Script::RegisterAll(machine);
    machine.Boot(Script::ScriptMain);
    machine.SetSidesShown(true);
    Eth::InputFrame input;
    input.cursor = input.cursorAbsolute = Eth::vector2(416.0f, 213.0f);   // novo_jogo's box (menu.esc)
    for (int i = 0; i < 240 && machine.Snapshot().sceneFile != "scenes/menu.esc"; ++i) machine.Frame(input);
    for (int i = 0; i < 200; ++i) machine.Frame(input);   // past the 3 s fade-in (LIVE_FADE_IN_TIME)
    const Eth::RenderSnapshot shown = machine.Snapshot();
    CHECK(shown.sceneFile == "scenes/menu.esc");
    CHECK(shown.sideMargin == Render::kWideMenuMargin);
    CHECK(machine.SceneSideMargin() == Render::kWideMenuMargin);

    // The backdrop is drawn: 22 white_ground tiles, ids of their own.
    std::size_t backdrop = 0;
    std::vector<int> screenIds;
    glm::vec2 newGame(-1.0f);
    for (const Eth::SpriteDraw& sprite : shown.sprites) {
        if (sprite.entityId >= Eth::Scene::kBackdropIdBase) {
            ++backdrop;
            CHECK(sprite.entityName == "white_ground.ent");
            CHECK(sprite.position.x < 0.0f || sprite.position.x > 1024.0f);
            continue;
        }
        screenIds.push_back(sprite.entityId);
        if (sprite.entityName == "novo_jogo") newGame = sprite.origin;
    }
    CHECK_EQ(backdrop, std::size_t{22});
    // The scripts see none of it.
    CHECK(Eth::SeekEntity(Eth::Scene::kBackdropIdBase) == nullptr);
    CHECK(machine.CurrentScene() != nullptr && machine.CurrentScene()->LastId() < 1000);

    // New Game where the pillarbox drew it: the 4:3 origin, the window's 240 px
    // margin added (1920 - 1024 x 1.40625 = 480, half each side).
    const Render::View view = Render::CameraRig::ComputeView(shown, glm::uvec2(1920u, 1080u), true);
    CHECK(view.viewportMin == glm::vec2(240.0f, 0.0f));
    CHECK(view.openSides == Render::kWideMenuMargin);
    CHECK(Render::CameraRig::Bars(view).empty());
    // menu.esc: (434, 209) at z 10, drawn 10 px up (ZAxisDirection (0,-1)),
    // less the centre of its 512x64 frame.
    CHECK(newGame == glm::vec2(434.0f - 256.0f, 209.0f - 10.0f - 32.0f));
    const glm::vec2 fraction = view.HudToFraction(newGame);
    CHECK(Near(fraction.x * 1920.0f, 240.0f + newGame.x * 1080.0f / 768.0f, 0.01f));

    // The panel's title, "Novo jogo", at rectPos + (10, 20) (menu.as:222),
    // rectPos.x = 1024 - 1024 x 0.382: in the window at 240 + 642.83 x 1.40625.
    bool title = false;
    for (const Eth::HudCmd& cmd : shown.hud) {
        if (cmd.kind != Eth::HudCmd::Kind::Text || cmd.text != "Novo jogo" || cmd.color != 0xFFCBCBE4u) continue;
        title = true;
        CHECK(Near(cmd.pos.x, 1024.0f - 1024.0f * (1.0f - 0.618f) + 10.0f, 0.01f));
        CHECK(Near(view.HudToFraction(cmd.pos).x * 1920.0f, 240.0f + cmd.pos.x * 1080.0f / 768.0f, 0.01f));
    }
    CHECK(title);
    // Its panel reaches the window's right edge.
    entt::registry registry;
    Render::TextureCache textures(kApp);
    Render::FontAtlas fonts;
    Render::Localization loc;
    loc.Load();
    Render::HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);
    std::vector<Supersonic::ScreenOverlay::Quad> quads;
    hud.Build(shown, view, quads);
    const bool toTheEdge = std::any_of(quads.begin(), quads.end(), [](const Supersonic::ScreenOverlay::Quad& q) {
        return q.texture.empty() && q.max.x >= 0.9999f && q.min.x >= 1680.0f / 1920.0f - 1e-3f;
    });
    CHECK(toTheEdge);

    // The same menu with the sides barred again (a 4:3 window): the screen's
    // own sprites, in the same order, and nothing past it.
    machine.SetSidesShown(false);
    machine.Frame(input);
    const Eth::RenderSnapshot barred = machine.Snapshot();
    CHECK(barred.sideMargin == 0.0f);
    std::vector<int> barredIds;
    for (const Eth::SpriteDraw& sprite : barred.sprites) barredIds.push_back(sprite.entityId);
    CHECK(barredIds == screenIds);
    CHECK(Render::CameraRig::Bars(Render::CameraRig::ComputeView(barred, glm::uvec2(1920u, 1080u), true)).size() == 2u);
}

} // namespace

int main() {
    if (!std::filesystem::exists(PENUMBRA_ORIGINAL_DIR "/main.as")) {
        std::printf("SKIP: the original is not at %s\n", PENUMBRA_ORIGINAL_DIR);
        return 77;
    }
    TestLocalization();
    TestTouchHints();
    TestFontStandIns();
    TestFontAtlas();
    TestEnhancedCredits();
    TestHudRenderer();
    TestWideMenuHud();
    TestWideMenuBackdrop();
    TestWideMenuScene();   // last: it boots the real game, whose globals outlive it
    return test::summary("test_pn_render_hud", 150);
}
