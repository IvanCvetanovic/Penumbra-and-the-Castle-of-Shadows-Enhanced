// The top layer's text: game/data/strings.json against the original's real
// strings (every literal that reaches DrawText, the heredocs, data.enml's lore
// and arena texts, the scenes' help signs and arena titles), the composed
// strings through the pattern rules, the control hints' touch wording (E16:
// every text that names a key has it, in both languages, and fits where it is
// drawn; with touch off not a byte changes), FontAtlas's layout of a two-line
// cp1252 string with an accent (when the system has the fonts), the quads
// HudRenderer builds on a bare registry, and E1's wide menus (Step 25): the
// rectangles that go on past the screen's sides, the backdrop each fixed-layout
// scene gets, and the real main menu in a 16:9 window. ENHANCEMENT E24: every
// language with its own file (game/data/strings/<id>.json) in each of those
// fits, its file's text drawn exactly as written (UTF-8, nothing turned into
// '?'), the English it falls back to where it has none, the characters of every
// file in the faces they are drawn from, Arabic's shaping and order against
// hand-worked goldens, every translatable text of every language in the room
// it has where the game draws it (tests/data/l10n_rooms.json), and a frame's
// texts growing a font atlas once.
// Script.hpp first: an engine header that reaches <windows.h> would turn the
// script API's DrawText into DrawTextA.
#include "script/Script.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "PenumbraLayer.hpp"   // E28
#include "TestHarness.hpp"
#include "core/Input.hpp"   // E28
#include "core/Json.hpp"
#include "eth/Defs.hpp"
#include "eth/Machine.hpp"
#include "eth/Snapshot.hpp"
#include "eth/Text.hpp"
#include "render/ArabicShaping.hpp"
#include "render/CameraRig.hpp"
#include "render/FontAtlas.hpp"
#include "render/HudRenderer.hpp"
#include "render/Localization.hpp"
#include "render/PhoneUi.hpp"
#include "render/TextureCache.hpp"
#include "render/TextureDecode.hpp"
#include "render/TouchControls.hpp"   // E28
#include "render/TouchEditor.hpp"   // E28
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

// E24: Translate hands back UTF-8; what a cp1252 literal is in it.
std::string U(const std::string& cp1252) { return Eth::Cp1252ToUtf8(cp1252); }

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

// E24: a language file as its translator wrote it, against everything
// strings.json asks of it: each string key, each translatable pattern, each
// touch key. An EMPTY value is a text not translated yet - drawn in English,
// which the spec allows - so it is printed, not failed; a MISSING key fails
// (TestLanguageFiles), and so does any text it leaves untranslated without
// saying so (CheckTranslated).
struct LanguageFile {
    Language language = Language::English;
    Supersonic::Json::Value root;
    int strings = 0;          // strings.json's keys the file fills
    int patterns = 0;         // the translatable patterns it fills
    int touch = 0;
    int stringsWanted = 0;
    int patternsWanted = 0;
    int touchWanted = 0;
    std::vector<std::string> missing;   // what it lacks: `strings "<key>"`, `patterns "<pt>"`, `touch "<key>"`
    std::vector<std::string> empty;     // what it lists with an empty value, the same way
    bool Complete() const { return missing.empty() && empty.empty(); }
};

std::string DataDir() { return std::filesystem::path(Render::Localization::DefaultPath()).parent_path().generic_string(); }

Supersonic::Json::Value ParseFile(const std::string& path) {
    std::string text = ReadBytes(path);
    if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);
    Supersonic::Json::Value root;
    std::string error;
    CHECK_MSG(Supersonic::Json::Parse(text, root, error), path + ": " + error);
    return root;
}

const Supersonic::Json::Value& StringsJson() {
    static const Supersonic::Json::Value base = ParseFile(Render::Localization::DefaultPath());
    return base;
}

// The keys of a strings.json-shaped object, its '_' notes left out.
std::set<std::string> KeysOf(const Supersonic::Json::Value& object) {
    std::set<std::string> keys;
    for (const auto& [key, value] : object.AsObject()) {
        if (!key.empty() && key[0] != '_') keys.insert(key);
    }
    return keys;
}

// The "pt" (UTF-8) of strings.json's patterns a language file words: all but
// the seven every language takes as they are (Localization::SharedPatterns,
// which hands back cp1252).
std::set<std::string> TranslatablePatterns() {
    Render::Localization loc;
    loc.Load();
    std::set<std::string> shared;
    for (const std::string& pt : loc.SharedPatterns()) shared.insert(Eth::Cp1252ToUtf8(pt));
    std::set<std::string> out;
    for (const Supersonic::Json::Value& entry : StringsJson()["patterns"].AsArray()) {
        if (entry.Has("pt") && shared.count(entry["pt"].AsString()) == 0) out.insert(entry["pt"].AsString());
    }
    return out;
}

// Every language but English and Portuguese that has a file, read once.
const std::vector<LanguageFile>& LanguageFiles() {
    static const std::vector<LanguageFile> files = [] {
        std::vector<LanguageFile> out;
        const Supersonic::Json::Value& base = StringsJson();
        const std::set<std::string> keys = KeysOf(base["strings"]);
        const std::set<std::string> patterns = TranslatablePatterns();
        const std::set<std::string> touch = KeysOf(base["touch"]);
        for (const Render::LanguageInfo& info : Render::kLanguages) {
            if (info.language == Language::English || info.language == Language::Portuguese) continue;
            const std::string path = DataDir() + "/strings/" + info.id + ".json";
            if (!std::filesystem::exists(path)) continue;   // TestLanguageFiles fails it
            LanguageFile file;
            file.language = info.language;
            file.root = ParseFile(path);
            file.stringsWanted = static_cast<int>(keys.size());
            file.patternsWanted = static_cast<int>(patterns.size());
            file.touchWanted = static_cast<int>(touch.size());
            // What it gives, key by key: filled, empty or not there at all.
            const auto classify = [&file](const std::string& what, const bool listed, const std::string& value,
                                          int& filled) {
                if (!listed) file.missing.push_back(what);
                else if (value.empty()) file.empty.push_back(what);
                else ++filled;
            };
            for (const std::string& key : keys) {
                const Supersonic::Json::Value& strings = file.root["strings"];
                classify("strings \"" + key + "\"", strings.Has(key), strings[key].AsString(), file.strings);
            }
            std::map<std::string, std::string> worded;
            for (const Supersonic::Json::Value& entry : file.root["patterns"].AsArray()) {
                worded[entry["pt"].AsString()] = entry["text"].AsString();
            }
            for (const std::string& pt : patterns) {
                const auto it = worded.find(pt);
                classify("patterns \"" + pt + "\"", it != worded.end(), it != worded.end() ? it->second : std::string(),
                     file.patterns);
            }
            for (const std::string& key : touch) {
                const Supersonic::Json::Value& wordings = file.root["touch"];
                classify("touch \"" + key + "\"", wordings.Has(key), wordings[key].AsString(), file.touch);
            }
            out.push_back(std::move(file));
        }
        return out;
    }();
    return files;
}

// A language file with every entry it leaves empty given strings.json's
// English, as JSON for Localization::LoadLanguageFromJson.
std::string FilledJson(const LanguageFile& file) {
    const Supersonic::Json::Value& base = StringsJson();
    const auto quoted = [](const std::string& text) { return "\"" + Supersonic::Json::Escape(text) + "\""; };
    std::string json = "{\"strings\": {";
    std::string comma;
    for (const auto& [key, value] : file.root["strings"].AsObject()) {
        const std::string text = value.AsString().empty() ? base["strings"][key].AsString() : value.AsString();
        json += comma + quoted(key) + ": " + quoted(text);
        comma = ", ";
    }
    json += "}, \"patterns\": [";
    comma.clear();
    for (const Supersonic::Json::Value& entry : file.root["patterns"].AsArray()) {
        std::string text = entry["text"].AsString();
        for (const Supersonic::Json::Value& english : base["patterns"].AsArray()) {
            if (text.empty() && english["pt"].AsString() == entry["pt"].AsString()) text = english["en"].AsString();
        }
        json += comma + "{\"pt\": " + quoted(entry["pt"].AsString()) + ", \"text\": " + quoted(text) + "}";
        comma = ", ";
    }
    json += "], \"touch\": {";
    comma.clear();
    for (const auto& [key, value] : file.root["touch"].AsObject()) {
        const std::string text = value.AsString().empty() ? base["touch"][key]["en"].AsString() : value.AsString();
        json += comma + quoted(key) + ": " + quoted(text);
        comma = ", ";
    }
    return json + "}}";
}

// Every language file as the game loads it, but each entry it leaves empty
// holding its English: a text this knows and the real files do not is one a
// file says "not yet" to, by an empty value - the only gap CheckTranslated
// excuses. Any other text a language lacks - a key missing from its file, a
// pattern keyed by a typo - is not excused.
const Render::Localization& Filled() {
    static Render::Localization filled;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        CHECK(filled.Load());
        for (const LanguageFile& file : LanguageFiles()) {
            std::string error;
            CHECK_MSG(filled.LoadLanguageFromJson(file.language, FilledJson(file), error),
                      std::string(Render::LanguageId(file.language)) + ": " + error);
        }
    }
    return filled;
}

// E24: the languages the fit checks measure - Portuguese, English, and every
// language with its own file (what it leaves empty is drawn in English, which
// fits already).
std::vector<Language> MeasuredLanguages(const Render::Localization& loc) {
    std::vector<Language> out = {Language::Portuguese, Language::English};
    for (const Render::LanguageInfo& info : Render::kLanguages) {
        if (info.language == Language::Portuguese || info.language == Language::English) continue;
        if (loc.HasLanguageFile(info.language)) out.push_back(info.language);
    }
    return out;
}

// A text laid out as HudRenderer lays it: translated, shaped and in drawing
// order, a right-to-left block's lines flush right.
Render::TextLayout Laid(Render::FontAtlas& fonts, const std::string& utf8, const Language language,
                        const std::string& face, const float size, const glm::vec2 pos = glm::vec2(0.0f)) {
    static Render::VisualText visual;
    const bool rightToLeft = Render::IsRightToLeft(language);
    return fonts.LayoutCodePoints(visual.Of(utf8, rightToLeft), face, size, pos,
                                  rightToLeft ? Render::LineAlign::Right : Render::LineAlign::Left);
}

// What an overflow says: the language, the key a translator looks for in the
// file (the Portuguese, cut short), the text drawn, what it measured, the limit.
std::string Overflow(const Language language, const std::string& cp1252Key, const std::string& utf8,
                     const float measured, const float limit) {
    std::string key = Eth::Cp1252ToUtf8(Render::Localization::Normalise(cp1252Key));
    if (key.size() > 60) key = key.substr(0, 57) + "...";
    return std::string(Render::LanguageId(language)) + " [" + key + "] \"" + utf8 + "\": " + std::to_string(measured) +
           " px, limit " + std::to_string(limit);
}

void CheckTranslated(const Render::Localization& loc, const std::string& portuguese, const std::string& where) {
    const bool known = loc.HasTranslation(portuguese);
    CHECK_MSG(known, where + ": no English for \"" + portuguese + "\"");
    // E24: every language with a file has every text, but for the entries its
    // file leaves empty on purpose (drawn in English; TestLanguageFiles prints
    // them). A key it lacks altogether is not excused.
    for (const Render::LanguageInfo& info : Render::kLanguages) {
        if (info.language == Language::English || info.language == Language::Portuguese) continue;
        if (!loc.HasLanguageFile(info.language)) continue;   // TestLanguageFiles fails a missing file
        const bool translated = loc.HasTranslation(portuguese, info.language);
        const bool leftEmpty = !translated && Filled().HasTranslation(portuguese, info.language);
        CHECK_MSG(translated || leftEmpty, where + ": no " + info.id + " for \"" + portuguese + "\"");
    }
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
    "Idioma",   // E24: the language chooser's label
    "Ativa movimento suave",   // E8's row
    "Desativa movimento suave",
    "Pausa ao perder o foco",   // E13's row
    "Continua sem o foco",
    "Ativa controles de toque",   // E20's row, on a phone
    "Desativa controles de toque",
};

// ENHANCEMENT E23: the display mode's lines and the refresh rate's row
// (game/script/videoModes.cpp, the layer's rate labels), the port's own
// Portuguese.
const char* const kE23Labels[] = {
    "Autom\xE1tico (melhor)",
    "Taxa de atualiza\xE7\xE3o",
    "Autom\xE1tica",
    "Autom\xE1tica (m\xE1xima)",
    "Vale para a tela cheia",
};

// ENHANCEMENT E28: the touch controls' editor's button (game/script/videoModes.cpp, drawn "[>] " + label     // E28
// without the added art) and its title (game/render/TouchEditor.cpp), the port's own Portuguese.            // E28
const char* const kE28Labels[] = {                                                                          // E28
    "Ajustar controles",                                                                                    // E28
    "Ajustar controles de toque",                                                                           // E28
};                                                                                                          // E28

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
    CHECK(loc.Translate("[\x95] Tela larga (widescreen)") == U("[\x95] Widescreen"));
    CHECK(loc.Translate("[ ] Jogador 2 s\xF3 no joystick") == "[ ] Player 2 on a joystick only");
    CHECK(loc.Translate("[ ] Portugu\xEAs") == U("[ ] Portugu\xEAs"));   // each language named in its own
    CHECK(loc.Translate("[\x95] English") == U("[\x95] English"));
    CHECK(loc.Translate("Volume da m\xFAsica") == "Music volume");
    CHECK(loc.Translate("[ ] Desativa movimento suave") == "[ ] Disable smooth motion");
    CHECK(loc.Translate("[\x95] Pausa ao perder o foco") == U("[\x95] Pause on focus loss"));
    CHECK(loc.Translate("[ ] Desativa controles de toque") == "[ ] Disable touch controls");   // E20
    CHECK(loc.HasTranslation("[<]"));
    CHECK(loc.HasTranslation("70%"));
    // E23: the display mode's lines and the refresh rate's values, as drawn.
    for (const char* label : kE23Labels) CheckTranslated(loc, label, "videoModes.cpp (E23)");
    CHECK(loc.Translate("[\x95] Autom\xE1tico (melhor)") == U("[\x95] Automatic (best)"));
    CHECK(loc.Translate("[ ] 1280x800") == "[ ] 1280x800");
    CHECK(loc.Translate("[\x95] 1920x1200 (nativa)") == U("[\x95] 1920x1200 (native)"));
    CHECK(loc.HasTranslation("[ ] 1280x800"));
    CHECK(loc.HasTranslation("[ ] 1920x1200 (nativa)"));
    CHECK(loc.Translate("Autom\xE1tica (165 Hz)") == "Automatic (165 Hz)");
    CHECK(loc.Translate("Autom\xE1tica (m\xE1xima)") == "Automatic (highest)");
    CHECK(loc.Translate("Autom\xE1tica") == "Automatic");
    CHECK(loc.Translate("60 Hz") == "60 Hz");
    CHECK(loc.HasTranslation("144 Hz"));
    CHECK(loc.Translate("Taxa de atualiza\xE7\xE3o") == "Refresh rate");
    CHECK(loc.Translate("Vale para a tela cheia") == "Applies in fullscreen");
    // E28: the editor's button and title have English and a line in each language file, so a missing one fails      // E28
    // here by name; the button as the script draws it without the art, through the bracket pattern.            // E28
    for (const char* label : kE28Labels) CheckTranslated(loc, label, "videoModes.cpp / TouchEditor.cpp (E28)");   // E28
    CHECK(loc.Translate("Ajustar controles") == "Adjust controls");                                          // E28
    CHECK(loc.Translate("Ajustar controles de toque") == "Adjust touch controls");                           // E28
    CHECK(loc.Translate("[>] Ajustar controles") == "[>] Adjust controls");                                  // E28
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
        CHECK(loc.Translate(text) != U(text));
    }
    // The bullet (0x95, cp1252 not Latin-1) survives into the English.
    CHECK(loc.Translate(Heredoc(menu, "como_jogar")).rfind(U("\x95"
                                                             "Controls"),
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
        CHECK(loc.Translate(value) != U(value));
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
    CHECK(loc.Translate("[\x95] Janela") == U("[\x95] Windowed"));
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
    CHECK(loc.Translate(versus, Language::Portuguese) == U(versus));
    CHECK(!loc.HasTranslation("Uma frase que n\xE3o existe"));
    CHECK(loc.Translate("Uma frase que n\xE3o existe") == U("Uma frase que n\xE3o existe"));

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
        CHECK(ptToday == U(text));
        // Off: exactly today's, in both languages.
        CHECK_MSG(loc.Translate(text, Language::Portuguese) == ptToday, hint->where);
        CHECK_MSG(loc.Translate(text, Language::English) == enToday, hint->where);

        // On: the touch wording in either language, no key named.
        loc.SetTouch(true);
        const std::string pt = loc.Translate(text, Language::Portuguese);
        const std::string en = loc.Translate(text, Language::English);
        CHECK_MSG(pt != ptToday && en != enToday, hint->where);
        CHECK_MSG(!NamesAKey(Eth::Utf8ToCp1252(pt)), hint->where + ": " + pt);
        CHECK_MSG(!NamesAKey(Eth::Utf8ToCp1252(en)), hint->where + ": " + en);
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
                const float width = fonts.LayoutUtf8(wording, "Arial", 30.0f, glm::vec2(10.0f, 70.0f)).width;
                CHECK_MSG(width > 0.0f && 10.0f + width <= 1024.0f, wording + ": " + std::to_string(width));
            } else if (hint->kind == Where::Enml) {
                // A lore sign: as many lines as the original's.
                CHECK_EQ(std::count(wording.begin(), wording.end(), '\n'), std::count(text.begin(), text.end(), '\n'));
            } else if (hint->kind == Where::Heredoc || (hint->kind == Where::Literal && hint->where == "menu.as")) {
                // (menu.as's one worded literal is Versus without a second
                // controller, E22, drawn by showData as the heredocs are.)
                // showData's panel: 391 px from x 633, the text at +10; from y 70
                // in Arial Narrow 25 on the 768 px menu (menu.as:217-229).
                const Render::TextLayout layout = fonts.LayoutUtf8(wording, "Arial Narrow", 25.0f, glm::vec2(0.0f));
                CHECK_MSG(layout.width > 0.0f && layout.width <= 381.0f, std::to_string(layout.width));
                CHECK_MSG(70.0f + static_cast<float>(layout.lines) * layout.lineHeight <= 768.0f,
                          std::to_string(layout.lines) + " lines");
            }
        }

        // E24: each language with its own file, its touch wording (or the
        // English it falls back to) where the original is drawn.
        for (const Language language : MeasuredLanguages(loc)) {
            if (language == Language::Portuguese || language == Language::English) continue;
            const std::string wording = loc.Translate(text, language);
            CHECK_MSG(!NamesAKey(Eth::Utf8ToCp1252(wording)), hint->where + " " + Render::LanguageId(language));
            if (hint->kind == Where::Sign) {
                const float width = Laid(fonts, wording, language, "Arial", 30.0f, glm::vec2(10.0f, 70.0f)).width;
                CHECK_MSG(width > 0.0f && 10.0f + width <= 1024.0f, Overflow(language, text, wording, 10.0f + width, 1024.0f));
            } else if (hint->kind == Where::Enml) {
                const auto lines = [](const std::string& t) { return std::count(t.begin(), t.end(), '\n'); };
                CHECK_MSG(lines(wording) <= std::max(lines(pt), lines(en)),
                          std::string(Render::LanguageId(language)) + " \"" + wording + "\": more lines than the sign has");
            } else if (hint->kind == Where::Heredoc || (hint->kind == Where::Literal && hint->where == "menu.as")) {
                const Render::TextLayout layout = Laid(fonts, wording, language, "Arial Narrow", 25.0f);
                CHECK_MSG(layout.width > 0.0f && layout.width <= 381.0f, Overflow(language, text, wording, layout.width, 381.0f));
                const float bottom = 70.0f + static_cast<float>(layout.lines) * layout.lineHeight;
                CHECK_MSG(bottom <= 768.0f, std::string(Render::LanguageId(language)) + " \"" + wording +
                                                "\": its bottom at y " + std::to_string(bottom) + ", the screen ends at 768");
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
    CHECK(howToEn.rfind(U("\x95"
                          "Touch controls"),
                        0) == 0);
    CHECK(loc.Translate(WithoutCr(howTo), Language::English) == howToEn);
    CHECK(loc.Translate(crlf + "\r", Language::English) == howToEn + "\n");
    CHECK(plain.Translate(crlf + "\r", Language::English) == plain.Translate(howTo, Language::English) + "\n");
    CHECK(loc.Translate(howTo, Language::Portuguese).rfind(U("\x95"
                                                             "Controles de toque"),
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
                const Render::TextLayout ours = standIns.LayoutCp1252(line, f.face, size, glm::vec2(0.0f));
                const Render::TextLayout theirs = system.LayoutCp1252(line, f.face, size, glm::vec2(0.0f));
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
    const float narrowWidth = standIns.LayoutCp1252("Carregando...", "Arial Narrow", 30.0f, glm::vec2(0.0f)).width;
    const float arialWidth = standIns.LayoutCp1252("Carregando...", "Arial", 30.0f, glm::vec2(0.0f)).width;
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
    const Render::TextLayout layout = fonts.LayoutCp1252("Ol\xE1\nmundo", "Arial Narrow", 25.0f, glm::vec2(10.7f, 20.2f));
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
    const Render::TextLayout plain = fonts.LayoutCp1252("ax", "Arial Narrow", 25.0f, glm::vec2(0.0f));
    const Render::TextLayout acute = fonts.LayoutCp1252("\xE1x", "Arial Narrow", 25.0f, glm::vec2(0.0f));
    CHECK_EQ(plain.glyphs.size(), std::size_t{2});
    CHECK_EQ(acute.glyphs.size(), std::size_t{2});
    if (plain.glyphs.size() == 2 && acute.glyphs.size() == 2) {
        CHECK_NEAR(plain.glyphs[1].pen.x, acute.glyphs[1].pen.x);
        CHECK(acute.glyphs[0].max.y - acute.glyphs[0].min.y > plain.glyphs[0].max.y - plain.glyphs[0].min.y);
        CHECK_NEAR(acute.glyphs[0].max.y, plain.glyphs[0].max.y);   // same baseline, same foot
    }

    // CR LF is one break; tabs go to stops of a fixed width.
    const Render::TextLayout crlf = fonts.LayoutCp1252("Ol\xE1\r\nmundo", "Arial Narrow", 25.0f, glm::vec2(10.7f, 20.2f));
    CHECK_EQ(crlf.lines, 2);
    CHECK_EQ(crlf.glyphs.size(), std::size_t{8});
    const Render::TextLayout oneTab = fonts.LayoutCp1252("\tb", "Arial Narrow", 25.0f, glm::vec2(0.0f));
    const Render::TextLayout twoTabs = fonts.LayoutCp1252("\t\tb", "Arial Narrow", 25.0f, glm::vec2(0.0f));
    if (oneTab.glyphs.size() == 1 && twoTabs.glyphs.size() == 1) {
        CHECK(oneTab.glyphs[0].pen.x > 0.0f);
        CHECK_NEAR(twoTabs.glyphs[0].pen.x, 2.0f * oneTab.glyphs[0].pen.x);
    }

    // ENHANCED: rasterised at twice the pixels, laid out in the same place.
    fonts.SetRasterScale(2.0f);
    const Render::TextLayout sharp = fonts.LayoutCp1252("Ol\xE1\nmundo", "Arial Narrow", 25.0f, glm::vec2(10.7f, 20.2f));
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
    const Render::TextLayout clock = fonts.LayoutCp1252("12:05", "Arial Narrow", 256.0f, glm::vec2(100.0f, 100.0f));
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
    CHECK(loc.Translate(credits, Language::Portuguese) == U(credits));
    CHECK(Render::Localization::Normalise("Cvetanovi\x8D\r\n") == "Cvetanovi\x8D\n");
    // English: the original's credits whole, then the name - its 0x8D read
    // from strings.json's UTF-8 U+0107 - and the role.
    CheckTranslated(loc, credits, "E21 credits");
    const std::string english = loc.Translate(credits, Language::English);
    const std::string originalEnglish = loc.Translate(Script::creditos, Language::English);
    CHECK(english.rfind(U("Andr\xE9 Santee\n -Programming"), 0) == 0);
    CHECK(english.compare(0, originalEnglish.size() - 1, originalEnglish, 0, originalEnglish.size() - 1) == 0);
    CHECK(english.find(U("-Taina Monclaire\n\nIvan Cvetanovi\x8D\n -Enhanced edition (Supersonic Engine)")) !=
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
        for (const std::string& text : {U(credits), english}) {
            const Render::TextLayout layout = fonts.LayoutUtf8(text, "Arial Narrow", 25.0f, glm::vec2(0.0f));
            std::printf("  E21 credits (%s, %s): %d lines, %.0f px wide, bottom at y %.0f\n",
                        system ? "system faces" : "stand-ins", text == english ? "en" : "pt", layout.lines,
                        layout.width, 70.0f + static_cast<float>(layout.lines) * layout.lineHeight);
            CHECK_EQ(layout.lines, 27);   // the original's 24, a blank line, the name, the role
            CHECK_MSG(layout.width > 0.0f && layout.width <= 381.0f, std::to_string(layout.width));
            CHECK_MSG(70.0f + static_cast<float>(layout.lines) * layout.lineHeight <= 768.0f,
                      std::to_string(layout.lines) + " lines");
        }
    }

    // E24: the panel in every language with its own file, the name in Latin
    // in each (strings.json's pattern and every file's keep it as it is).
    for (const bool system : {true, false}) {
        Render::FontAtlas fonts;
        fonts.SetSystemFontsEnabled(system);
        if (fonts.FaceFile("Arial Narrow").empty()) continue;
        for (const Language language : MeasuredLanguages(loc)) {
            if (language == Language::Portuguese || language == Language::English) continue;
            const std::string text = loc.Translate(credits, language);
            CHECK_MSG(text.find(U("Ivan Cvetanovi\x8D")) != std::string::npos, Render::LanguageId(language));
            const Render::TextLayout layout = Laid(fonts, text, language, "Arial Narrow", 25.0f);
            const float bottom = 70.0f + static_cast<float>(layout.lines) * layout.lineHeight;
            std::printf("  E21 credits (%s, %s): %d lines, %.0f px wide, bottom at y %.0f\n",
                        system ? "system faces" : "stand-ins", Render::LanguageId(language), layout.lines, layout.width,
                        bottom);
            CHECK_MSG(layout.width > 0.0f && layout.width <= 381.0f, Overflow(language, "creditos", "the credits", layout.width, 381.0f));
            CHECK_MSG(bottom <= 768.0f, Overflow(language, "creditos", "the credits' bottom", bottom, 768.0f));
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
        const Render::TextLayout plain = fonts.LayoutCp1252("cx", "Arial Narrow", 25.0f, glm::vec2(0.0f));
        const Render::TextLayout acute = fonts.LayoutCp1252("\x8Dx", "Arial Narrow", 25.0f, glm::vec2(0.0f));
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

// E23: the display mode's lines fit the list's column and the rate's values
// fit their box, in both languages, with the Windows faces where this machine
// has them and with the stand-ins (videoModes.cpp: the list from x 30, where
// the switches begin at x 255; the rate's value from x 584 in a 200 px box
// whose "[>]" starts at x 780; Arial Narrow 25).
void TestDisplayModeRowsFit() {
    Render::Localization loc;
    CHECK(loc.Load());
    const std::vector<std::string> lines = {
        "[\x95] Autom\xE1tico (melhor)", "[ ] 3840x2400 (nativa)", "[ ] 15360x8640", "[\x95] 1920x1200 (nativa)"};
    const std::vector<std::string> values = {"Autom\xE1tica (m\xE1xima)", "Autom\xE1tica (1000 Hz)", "Autom\xE1tica",
                                             "1000 Hz"};
    for (const bool system : {true, false}) {
        Render::FontAtlas fonts;
        fonts.SetSystemFontsEnabled(system);
        if (fonts.FaceFile("Arial Narrow").empty()) continue;
        for (const Language language : MeasuredLanguages(loc)) {
            float widestLine = 0.0f;
            for (const std::string& line : lines) {
                const std::string text = loc.Translate(line, language);
                const float width = Laid(fonts, text, language, "Arial Narrow", 25.0f).width;
                widestLine = std::max(widestLine, width);
                CHECK_MSG(width > 0.0f && 30.0f + width <= 230.0f, Overflow(language, line, text, 30.0f + width, 230.0f));
            }
            float widestValue = 0.0f;
            for (const std::string& value : values) {
                const std::string text = loc.Translate(value, language);
                const float width = Laid(fonts, text, language, "Arial Narrow", 25.0f).width;
                widestValue = std::max(widestValue, width);
                CHECK_MSG(width > 0.0f && 4.0f + width < 200.0f, Overflow(language, value, text, 4.0f + width, 200.0f));
            }
            std::printf("  E23 (%s, %s): widest list line %.0f px (ends at x %.0f, the switches at 255); "
                        "widest rate %.0f px (ends at x %.0f, the [>] box at 780)\n",
                        system ? "system faces" : "stand-ins", Render::LanguageId(language), widestLine,
                        30.0f + widestLine, widestValue, 584.0f + widestValue);
        }
    }
}

// E24: the languages - strings.json's list against Languages.hpp, the chooser
// drawing each name the same in every language (and in its box), the chooser's
// label, and the seven patterns every language shares.
void TestLanguages() {
    Render::Localization loc;
    CHECK(loc.Load());
    const Supersonic::Json::Value root = ParseFile(Render::Localization::DefaultPath());
    const auto& listed = root["languages"].AsArray();
    CHECK_EQ(listed.size(), Render::kLanguageCount);
    for (std::size_t i = 0; i < listed.size() && i < Render::kLanguageCount; ++i) {
        const Render::LanguageInfo& info = Render::kLanguages[i];
        CHECK_MSG(listed[i]["id"].AsString() == info.id, listed[i]["id"].AsString() + " at " + std::to_string(i));
        CHECK_MSG((listed[i].Has("rtl") && listed[i]["rtl"].AsBool()) == info.rightToLeft, info.id);
        CHECK_MSG(!listed[i]["name"].AsString().empty(), info.id);
        CHECK(loc.LanguageName(info.language) == listed[i]["name"].AsString());
        Language parsed = Language::English;
        CHECK(Render::LanguageFromId(info.id, parsed) && parsed == info.language);
    }
    CHECK(loc.LanguageName(Language::Portuguese) == U("Portugu\xEAs"));
    CHECK(loc.LanguageName(Language::Russian) == "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9");
    Language none = Language::Arabic;
    CHECK(!Render::LanguageFromId("zh", none) && none == Language::Arabic);

    // Every name, in every language, is itself: no language translates another's.
    for (const Render::LanguageInfo& named : Render::kLanguages) {
        const std::string key = Render::Localization::LanguageNameKey(named.language);
        CHECK(key == std::string("{language:") + named.id + "}");
        for (const Render::LanguageInfo& drawnIn : Render::kLanguages) {
            CHECK_MSG(loc.Translate(key, drawnIn.language) == loc.LanguageName(named.language),
                      std::string(named.id) + " in " + drawnIn.id);
            CHECK(loc.HasTranslation(key, drawnIn.language));
        }
    }
    CHECK(loc.Translate("{language:zz}", Language::English) == "{language:zz}");   // not a language: as it is

    // The chooser's label, and each name in its row (videoModes.cpp: the value
    // box is 176 px, the text 4 px in; the label on the switch's 256 px line).
    CHECK(loc.Translate("Idioma", Language::English) == "Language");
    CHECK(loc.Translate("Idioma", Language::Portuguese) == "Idioma");
    for (const bool system : {true, false}) {
        Render::FontAtlas fonts;
        fonts.SetSystemFontsEnabled(system);
        if (fonts.FaceFile("Arial Narrow").empty()) continue;
        for (const Language language : MeasuredLanguages(loc)) {
            const std::string label = loc.Translate("Idioma", language);
            const float labelWidth = Laid(fonts, label, language, "Arial Narrow", 25.0f).width;
            CHECK_MSG(labelWidth > 0.0f && labelWidth <= 256.0f, Overflow(language, "Idioma", label, labelWidth, 256.0f));
            float widest = 0.0f;
            for (const Render::LanguageInfo& named : Render::kLanguages) {
                const std::string name = loc.Translate(Render::Localization::LanguageNameKey(named.language), language);
                const float width = Laid(fonts, name, language, "Arial Narrow", 25.0f).width;
                widest = std::max(widest, width);
                CHECK_MSG(width > 0.0f && 4.0f + width < 176.0f, Overflow(language, Render::Localization::LanguageNameKey(named.language), name, 4.0f + width, 176.0f));
            }
            // E27: automatic, the chooser's first entry (the script's "Autom\xE1tica").
            const std::string automatic = loc.Translate("Autom\xE1tica", language);
            const float automaticWidth = Laid(fonts, automatic, language, "Arial Narrow", 25.0f).width;
            CHECK_MSG(automaticWidth > 0.0f && 4.0f + automaticWidth < 176.0f,
                      Overflow(language, "Autom\xE1tica", automatic, 4.0f + automaticWidth, 176.0f));
            if (language == Language::English) {
                std::printf("  E24 chooser (%s): the widest name %.0f px of the 172 the box leaves\n",
                            system ? "system faces" : "stand-ins", widest);
            }
        }
    }

    // The patterns every language draws from strings.json as they are: the
    // spec's seven, numbers and markers only - and none of the nine a
    // translator words.
    const std::vector<std::string> shared = loc.SharedPatterns();
    const std::vector<std::string> expected = {"[{any}] {text}", "hp: {int}", "mp: {int}", "lv: {int}",
                                               "{int}x{int}x{int}", "{int}x{int}", "{int} Hz", "{int}%"};   // E25's zoom
    CHECK(shared == expected);
    CHECK_EQ(loc.PatternCount(), std::size_t{17});
}

// E24: the rules of a language file, on documents of our own (the real files
// are being written): its text, the English where it has none, patterns shared
// or its own, the touch wording, UTF-8 kept whole, and the caches of two
// languages kept apart.
void TestLanguageFallback() {
    Render::Localization loc;
    std::string error;
    CHECK(loc.Load());
    // German and French of our own, over whatever files exist.
    CHECK_MSG(loc.LoadLanguageFromJson(Language::German,
                                       "{\"strings\": {\"Novo jogo\": \"Neues Spiel\", \"Configura\xC3\xA7\xC3\xB5"
                                       "es\": \"\","
                                       " \"Janela\": \"Fenster\"},"
                                       " \"patterns\": [{\"pt\": \"Jogador {int} \xC3\xA9 o vencedor!\","
                                       " \"text\": \"Spieler {1} gewinnt!\"}],"
                                       " \"touch\": {\"Golpe de espada: tecla 'S'\": \"Schwerthieb: die Schwerttaste\"}}",
                                       error),
              error);
    CHECK_MSG(loc.LoadLanguageFromJson(Language::French,
                                       "{\"strings\": {\"Novo jogo\": \"Nouvelle partie\"}, \"patterns\": [], \"touch\": {}}",
                                       error),
              error);
    CHECK(loc.HasLanguageFile(Language::German));
    CHECK(!loc.LoadLanguageFromJson(Language::English, "{}", error));   // strings.json's

    CHECK(loc.Translate("Novo jogo", Language::German) == "Neues Spiel");
    CHECK(loc.HasTranslation("Novo jogo", Language::German));
    // Empty, or missing: the English.
    CHECK(loc.Translate("Configura\xE7\xF5" "es", Language::German) == "Settings");
    CHECK(!loc.HasTranslation("Configura\xE7\xF5" "es", Language::German));
    CHECK(loc.Translate("Sair do jogo", Language::German) == loc.Translate("Sair do jogo", Language::English));
    // Its own pattern, the blank lines kept; a pattern it lacks, the English.
    CHECK(loc.Translate("\n\nJogador 2 \xE9 o vencedor!\n", Language::German) == "\n\nSpieler 2 gewinnt!\n");
    CHECK(loc.Translate("\xC9 necess\xE1rio ter 25 mana para este combo", Language::German) ==
          "You need 25 mana for this combo");
    CHECK(!loc.HasTranslation("\xC9 necess\xE1rio ter 25 mana para este combo", Language::German));
    // A shared pattern is no gap; its {text} is the language's.
    CHECK(loc.Translate("[\x95] Janela", Language::German) == U("[\x95] Fenster"));
    CHECK(loc.HasTranslation("[\x95] Janela", Language::German));
    CHECK(loc.Translate("hp: 75", Language::German) == "hp: 75");
    CHECK(loc.HasTranslation("12:05", Language::German));
    // Not in English either: the Portuguese, in UTF-8.
    CHECK(loc.Translate("Uma frase que n\xE3o existe", Language::German) == U("Uma frase que n\xE3o existe"));

    // The memo is per language: de, fr, de again, each its own.
    for (int round = 0; round < 2; ++round) {
        CHECK(loc.Translate("Novo jogo", Language::German) == "Neues Spiel");
        CHECK(loc.Translate("Novo jogo", Language::French) == "Nouvelle partie");
        loc.SetLanguage(Language::French);
        CHECK(loc.Translate("Novo jogo") == "Nouvelle partie");
        loc.SetLanguage(Language::German);
        CHECK(loc.Translate("Novo jogo") == "Neues Spiel");
    }
    CHECK(!loc.RightToLeft());
    loc.SetLanguage(Language::Arabic);
    CHECK(loc.RightToLeft());
    loc.SetLanguage(Language::English);

    // Touch: the language's own wording, else the English one.
    loc.SetTouch(true);
    CHECK(loc.Translate("Golpe de espada: tecla 'S'", Language::German) == "Schwerthieb: die Schwerttaste");
    CHECK(loc.HasTouchVariant("Golpe de espada: tecla 'S'", Language::German));
    CHECK(!loc.HasTouchVariant("Golpe de espada: tecla 'S'", Language::French));
    CHECK(loc.Translate("Golpe de espada: tecla 'S'", Language::French) == "Sword strike: the sword button");
    loc.SetTouch(false);
    CHECK(loc.Translate("Golpe de espada: tecla 'S'", Language::German) == "Sword strike: 'S' key");

    // Beyond cp1252: Turkish, Cyrillic, Japanese and Arabic come out as the
    // file wrote them, not as '?'.
    const std::string russian = "\xD0\x9D\xD0\xBE\xD0\xB2\xD0\xB0\xD1\x8F \xD0\xB8\xD0\xB3\xD1\x80\xD0\xB0";
    const std::string turkish = "Yeni oyun \xC4\x9F\xC4\xB1\xC4\xB0\xC5\x9F";
    const std::string japanese = "\xE6\x96\xB0\xE3\x81\x97\xE3\x81\x84\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0";
    const std::string arabic = "\xD9\x84\xD8\xB9\xD8\xA8\xD8\xA9 \xD8\xAC\xD8\xAF\xD9\x8A\xD8\xAF\xD8\xA9";
    const std::pair<Language, std::string> unicode[] = {
        {Language::Russian, russian}, {Language::Turkish, turkish}, {Language::Japanese, japanese},
        {Language::Arabic, arabic}};
    for (const auto& [language, text] : unicode) {
        CHECK(loc.LoadLanguageFromJson(language, "{\"strings\": {\"Novo jogo\": \"" + text + "\"}}", error));
        CHECK_MSG(loc.Translate("Novo jogo", language) == text, Render::LanguageId(language));
        CHECK(loc.Translate("Novo jogo", language).find('?') == std::string::npos);
    }
}

// E24: the translators' files as they are. There is one for every language
// but English and Portuguese, and it has every key strings.json has (an empty
// value is allowed: drawn in English, and printed). Every text a file gives is
// drawn exactly as written; a pattern is one of strings.json's own and names
// each of its captures.
void TestLanguageFiles() {
    Render::Localization loc;
    CHECK(loc.Load());
    const auto value = [](const std::string& utf8) {
        // Localization's own normalising of a value: LF, no spaces before a
        // break, trimmed.
        std::string text = Render::Localization::Normalise(utf8);
        const std::size_t begin = text.find_first_not_of(" \n\t");
        const std::size_t end = text.find_last_not_of(" \n\t");
        return begin == std::string::npos ? std::string() : text.substr(begin, end - begin + 1);
    };
    // The files are there: without one, every check that loops over the
    // languages with a file would quietly run over fewer.
    for (const Render::LanguageInfo& info : Render::kLanguages) {
        if (info.language == Language::English || info.language == Language::Portuguese) continue;
        const std::string path = DataDir() + "/strings/" + info.id + ".json";
        CHECK_MSG(std::filesystem::exists(path), "no " + path + ": " + info.id + " would be drawn in English");
        CHECK_MSG(loc.HasLanguageFile(info.language), std::string(info.id) + ".json was not loaded");
    }
    CHECK_MSG(!LanguageFiles().empty(), "no language files in " + DataDir() + "/strings");
    CHECK_EQ(LanguageFiles().size(), Render::kLanguageCount - 2);
    const std::set<std::string> translatable = TranslatablePatterns();
    CHECK_EQ(translatable.size(), std::size_t{9});
    for (const LanguageFile& file : LanguageFiles()) {
        const char* id = Render::LanguageId(file.language);
        CHECK_MSG(loc.HasLanguageFile(file.language), id);
        std::printf("  %s.json: %d of %d strings, %d of %d patterns, %d of %d touch wordings (the rest in English)%s\n",
                    id, file.strings, file.stringsWanted, file.patterns, file.patternsWanted, file.touch,
                    file.touchWanted, file.Complete() ? "; complete" : "");
        // Every key strings.json has; one left empty is drawn in English, said here.
        for (const std::string& what : file.missing) CHECK_MSG(false, std::string(id) + ".json lacks " + what);
        for (const std::string& what : file.empty) {
            std::printf("    %s.json leaves %s empty: drawn in English\n", id, what.c_str());
        }
        for (const auto& [key, text] : file.root["strings"].AsObject()) {
            const std::string written = text.AsString();
            if (key.empty() || key[0] == '_' || written.empty()) continue;
            const std::string cp1252 = Eth::Utf8ToCp1252(key);
            CHECK_MSG(Eth::Cp1252ToUtf8(cp1252) == key, std::string(id) + ": a key cp1252 cannot hold: " + key);
            CHECK_MSG(loc.HasTranslation(cp1252, Language::English), std::string(id) + ": not a strings.json key: " + key);
            const std::string translated = loc.Translate(cp1252, file.language);
            CHECK_MSG(translated == value(written), std::string(id) + " \"" + key + "\" is drawn as \"" + translated + "\"");
            CHECK_MSG(std::count(translated.begin(), translated.end(), '?') == std::count(written.begin(), written.end(), '?'),
                      std::string(id) + ": a '?' in \"" + translated + "\"");
        }
        for (const Supersonic::Json::Value& entry : file.root["patterns"].AsArray()) {
            const std::string pt = entry["pt"].AsString();
            const std::string text = entry["text"].AsString();
            // A pattern keyed by anything else - a typo, a shared pattern - is
            // never used: its text would be drawn in English.
            CHECK_MSG(translatable.count(pt) != 0,
                      std::string(id) + " pattern \"" + pt + "\": not one of strings.json's translatable patterns");
            if (text.empty()) continue;
            std::size_t captures = 0;
            for (const char* slot : {"{int}", "{any}", "{text}"}) {
                for (std::size_t at = pt.find(slot); at != std::string::npos; at = pt.find(slot, at + 1)) ++captures;
            }
            for (std::size_t i = 1; i <= captures; ++i) {
                CHECK_MSG(text.find("{" + std::to_string(i) + "}") != std::string::npos,
                          std::string(id) + " \"" + pt + "\": no {" + std::to_string(i) + "}");
            }
            CHECK_MSG(text.find("{" + std::to_string(captures + 1) + "}") == std::string::npos,
                      std::string(id) + " \"" + pt + "\": a placeholder past its captures");
        }
        Render::Localization touch;
        touch.Load();
        touch.SetTouch(true);
        for (const auto& [key, text] : file.root["touch"].AsObject()) {
            if (key.empty() || key[0] == '_' || text.AsString().empty()) continue;
            CHECK_MSG(touch.HasTouchVariant(Eth::Utf8ToCp1252(key), file.language), std::string(id) + " touch \"" + key + "\"");
            CHECK_MSG(touch.Translate(Eth::Utf8ToCp1252(key), file.language) == value(text.AsString()),
                      std::string(id) + " touch \"" + key + "\"");
        }
        CHECK_MSG(Filled().HasLanguageFile(file.language), id);
    }

    // CheckTranslated's excuse, on a file of our own: what a file leaves
    // empty its filled copy knows (the English), what it lacks neither does.
    const std::string document = "{\"strings\": {\"Novo jogo\": \"\"},"
                                 " \"patterns\": [{\"pt\": \"Jogador {int} \xC3\xA9 o vencedor!\", \"text\": \"\"}],"
                                 " \"touch\": {\"Golpe de espada: tecla 'S'\": \"\"}}";
    LanguageFile own;
    own.language = Language::German;
    std::string error;
    CHECK_MSG(Supersonic::Json::Parse(document, own.root, error), error);
    Render::Localization raw;
    Render::Localization excused;
    CHECK(raw.Load() && excused.Load());
    CHECK_MSG(raw.LoadLanguageFromJson(Language::German, document, error), error);
    CHECK_MSG(excused.LoadLanguageFromJson(Language::German, FilledJson(own), error), error);
    CHECK(!raw.HasTranslation("Novo jogo", Language::German));
    CHECK(excused.HasTranslation("Novo jogo", Language::German));
    CHECK(!raw.HasTranslation("\n\nJogador 2 \xE9 o vencedor!\n", Language::German));
    CHECK(excused.HasTranslation("\n\nJogador 2 \xE9 o vencedor!\n", Language::German));
    CHECK(!excused.HasTranslation("Sair do jogo", Language::German));   // not in the file: no excuse
    CHECK(!raw.HasTouchVariant("Golpe de espada: tecla 'S'", Language::German));
    CHECK(excused.HasTouchVariant("Golpe de espada: tecla 'S'", Language::German));
}

// E24: an environment variable, "" when unset (getenv without MSVC's C4996,
// as FontAtlas::FontsDirectory reads WINDIR).
std::string EnvironmentVariable(const char* name) {
#if defined(_MSC_VER)
    char* value = nullptr;
    std::size_t length = 0;
    std::string out;
    if (_dupenv_s(&value, &length, name) == 0 && value != nullptr) out = value;
    std::free(value);
    return out;
#else
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string();
#endif
}

// E24: a text's ROOM where the game draws it (tests/data/l10n_rooms.json,
// whose _about has the rules).
struct Room {
    std::string section;     // "strings", "patterns" or "touch", as strings.json has them
    std::string key;         // UTF-8, as the section keys it
    std::string source;      // cp1252: what the script draws - the prefix and the key, or the sample
    std::string face;
    float size = 0.0f;
    float maxWidth = 0.0f;   // logical px, each line
    int maxLines = 0;
    bool visual = false;     // max(pt, en) x 1.15 rather than a box
    float cap = 0.0f;        // a visual room's hard limit (0: none)
};

// A key as one line of output: its breaks shown as \n, cut short.
std::string ShownKey(const Room& room) {
    std::u32string codePoints = Eth::Utf8ToCodePoints(room.key);
    const bool cut = codePoints.size() > 48;
    if (cut) codePoints.resize(45);
    std::string out;
    for (const char32_t c : codePoints) {
        out += c == U'\n' ? std::string("\\n") : Eth::CodePointsToUtf8(std::u32string(1, c));
    }
    return (room.section == "touch" ? "touch \"" : "\"") + out + (cut ? "...\"" : "\"");
}

// Each line's width as the HUD lays it out, and which is the widest. A line
// alone is laid out as it is inside its block (the pen starts again on every
// line; an Arabic line is shaped and ordered by itself either way).
struct LineWidths {
    std::vector<float> widths;
    float widest = 0.0f;
};

LineWidths MeasureLines(Render::FontAtlas& fonts, const std::string& utf8, const Language language,
                        const std::string& face, const float size) {
    LineWidths out;
    std::size_t begin = 0;
    for (;;) {
        const std::size_t end = utf8.find('\n', begin);
        const std::string line = utf8.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
        const float width = line.empty() ? 0.0f : Laid(fonts, line, language, face, size).width;
        out.widths.push_back(width);
        out.widest = std::max(out.widest, width);
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return out;
}

// ENHANCEMENT E24: every translatable text in its room - strings.json's
// strings, its nine translatable patterns (each through a sample the script
// would compose) and its touch wordings - in every language, laid out with the
// game's own FontAtlas (this platform's faces: the Windows ones where they
// are, the stand-ins elsewhere; Japanese and Arabic from the bundled Noto). A
// translation may use all the room there is, not only the Portuguese's or the
// English's width. Every line past its room is printed, one line each,
// language by language:
//   room <lang> "<key>" line N: <w> px > <max> px (<face> <size>)
// Portuguese and English must fit (a room they overflow is a wrong room).
// PN_ROOM_REPORT=<id> (or all) prints each text's widest line against its room.
void TestRooms() {
    const std::string path = std::string(PENUMBRA_TESTS_DATA_DIR) + "/l10n_rooms.json";
    const Supersonic::Json::Value table = ParseFile(path);
    Render::Localization loc;
    CHECK(loc.Load());
    Render::Localization touch;
    CHECK(touch.Load());
    touch.SetTouch(true);

    // Every text has a room and every room a text: a new string cannot go
    // unmeasured, and a room cannot outlive its string.
    const Supersonic::Json::Value& base = StringsJson();
    const std::set<std::string> wanted[] = {KeysOf(base["strings"]), TranslatablePatterns(), KeysOf(base["touch"])};
    const char* const sections[] = {"strings", "patterns", "touch"};
    std::vector<Room> rooms;
    int notDrawn = 0;
    for (std::size_t s = 0; s < 3; ++s) {
        const std::string section = sections[s];
        const std::set<std::string> listed = KeysOf(table[section]);
        for (const std::string& key : wanted[s]) {
            CHECK_MSG(listed.count(key) != 0, path + ": no room for " + section + " \"" + key + "\"");
        }
        for (const std::string& key : listed) {
            CHECK_MSG(wanted[s].count(key) != 0,
                      path + ": a room for " + section + " \"" + key + "\", which strings.json does not have");
        }
        for (const std::string& key : table[section].OrderedKeys()) {
            if (key.empty() || key[0] == '_') continue;
            const Supersonic::Json::Value& entry = table[section][key];
            CHECK_MSG(!entry["source"].AsString().empty() && !entry["why"].AsString().empty(),
                      section + " \"" + key + "\": a room says where and why");
            if (!entry["maxWidth"].IsNumber()) {
                ++notDrawn;   // null: never drawn
                continue;
            }
            Room room;
            room.section = section;
            room.key = key;
            room.face = entry["face"].AsString();
            room.size = entry["size"].AsFloat();
            room.maxWidth = entry["maxWidth"].AsFloat();
            // E27: with the added art a row's "[x] " is a 26 px check box and its gap, where the model
            // measures the brackets at about 24: the label has that much less room (videoModes.cpp,
            // switch.cpp: kMarkWidth). The rooms themselves are unchanged, so the rows still answer to
            // the original's geometry.
            // A row whose column is tight keeps the brackets' own width (the mode list's 24 px box:
            // "markReserve": 0).
            if (entry["prefix"].AsString().rfind("[", 0) == 0)
                room.maxWidth -= entry["markReserve"].IsNumber() ? entry["markReserve"].AsFloat() : 2.5f;
            room.maxLines = static_cast<int>(entry["maxLines"].AsNumber());
            room.visual = entry["rule"].AsString() == "visual";
            room.cap = entry["cap"].AsFloat();
            if (section == "patterns") {
                room.source = Eth::Utf8ToCp1252(entry["sample"].AsString());
                // The sample is the pattern's, as the script composes it: known.
                CHECK_MSG(loc.HasTranslation(room.source), path + ": the sample of \"" + key + "\" has no English");
            } else {
                room.source = Eth::Utf8ToCp1252(entry["prefix"].AsString() + key);
            }
            CHECK_MSG((room.face == "Arial Narrow" || room.face == "Arial") && room.size > 0.0f &&
                          room.maxWidth > 0.0f && room.maxLines > 0,
                      section + " \"" + key + "\"");
            rooms.push_back(std::move(room));
        }
    }
    std::printf("  rooms: %zu texts measured, %d never drawn (%s)\n", rooms.size(), notDrawn, path.c_str());
    CHECK(rooms.size() > 100);

    // A visual room is max(pt, en) x 1.15 and their line count, from
    // strings.json as it is now, measured with the stand-ins the table was
    // made with: a changed English shows here as a stale room.
    Render::FontAtlas standIns;
    standIns.SetSystemFontsEnabled(false);
    for (const Room& room : rooms) {
        if (!room.visual) continue;
        const Render::Localization& from = room.section == "touch" ? touch : loc;
        float widest = 0.0f;
        std::size_t lines = 0;
        for (const Language language : {Language::Portuguese, Language::English}) {
            const LineWidths measured =
                MeasureLines(standIns, from.Translate(room.source, language), language, room.face, room.size);
            widest = std::max(widest, measured.widest);
            lines = std::max(lines, measured.widths.size());
        }
        float expected = std::floor(widest * 1.15f + 0.5f);
        if (room.cap > 0.0f) expected = std::min(expected, room.cap);
        CHECK_MSG(std::fabs(expected - room.maxWidth) <= 1.0f,
                  ShownKey(room) + ": a visual room of " + std::to_string(room.maxWidth) +
                      " px, max(pt, en) x 1.15 is now " + std::to_string(expected));
        CHECK_MSG(static_cast<int>(lines) == room.maxLines,
                  ShownKey(room) + ": pt/en have " + std::to_string(lines) + " lines");
    }

    // Every language, laid out as the game lays it out here.
    Render::FontAtlas fonts;
    const std::string report = EnvironmentVariable("PN_ROOM_REPORT");
    for (const Language language : MeasuredLanguages(loc)) {
        const char* id = Render::LanguageId(language);
        const bool reported = report == "all" || report == id;
        int overflowing = 0;
        for (const Room& room : rooms) {
            const Render::Localization& from = room.section == "touch" ? touch : loc;
            const std::string text = from.Translate(room.source, language);
            const LineWidths measured = MeasureLines(fonts, text, language, room.face, room.size);
            const int lines = static_cast<int>(measured.widths.size());
            bool over = false;
            for (std::size_t i = 0; i < measured.widths.size(); ++i) {
                if (measured.widths[i] <= room.maxWidth) continue;
                over = true;
                std::printf("  room %s %s line %zu: %.0f px > %.0f px (%s %.0f)\n", id, ShownKey(room).c_str(), i + 1,
                            measured.widths[i], room.maxWidth, room.face.c_str(), room.size);
            }
            if (lines > room.maxLines) {
                over = true;
                std::printf("  room %s %s: %d lines > %d lines (%s %.0f)\n", id, ShownKey(room).c_str(), lines,
                            room.maxLines, room.face.c_str(), room.size);
            }
            if (over) ++overflowing;
            if (reported) {
                std::printf("  room-report %s %s: widest %.0f of %.0f px (%+.0f), %d of %d lines (%s %.0f)\n", id,
                            ShownKey(room).c_str(), measured.widest, room.maxWidth, room.maxWidth - measured.widest,
                            lines, room.maxLines, room.face.c_str(), room.size);
            }
        }
        if (language == Language::Portuguese || language == Language::English) {
            CHECK_MSG(overflowing == 0, std::string(id) + ": " + std::to_string(overflowing) +
                                            " texts past their room - the original and the English fit where they are "
                                            "drawn, so the room in " + path + " is wrong, not the text");
        } else {
            CHECK_MSG(overflowing == 0, std::string(id) + ": " + std::to_string(overflowing) +
                                            " texts past their room (the 'room " + id + "' lines above)");
        }
    }
}

// E24: every character a language file draws - and the language names - is in
// the face it is drawn from (FontAtlas routes Japanese and Arabic to the
// bundled Noto faces, everything else to the text's own), in all four faces the
// scripts name, with the stand-ins and with this machine's Windows faces;
// Arabic in the forms it is shaped into. A kanji the Japanese face lacks means
// tools/l10n/make_fonts.py was not rerun after ja.json changed.
void TestFontCoverage() {
    std::vector<std::pair<std::string, std::u32string>> texts;   // (where, code points)
    Render::Localization loc;
    CHECK(loc.Load());
    for (const Render::LanguageInfo& info : Render::kLanguages) {
        const std::u32string name = Eth::Utf8ToCodePoints(loc.LanguageName(info.language));
        texts.emplace_back(std::string("the name of ") + info.id, name);
        texts.emplace_back(std::string("the name of ") + info.id + ", shaped", Render::ArabicShaping::Shape(name));
    }
    const auto add = [&texts](const std::string& where, const std::string& utf8) {
        const std::u32string codePoints = Eth::Utf8ToCodePoints(utf8);
        texts.emplace_back(where, codePoints);
        texts.emplace_back(where + ", shaped", Render::ArabicShaping::Shape(codePoints));
    };
    for (const LanguageFile& file : LanguageFiles()) {
        const std::string id = Render::LanguageId(file.language);
        for (const auto& [key, text] : file.root["strings"].AsObject()) add(id + " \"" + key + "\"", text.AsString());
        for (const Supersonic::Json::Value& entry : file.root["patterns"].AsArray()) add(id + " pattern", entry["text"].AsString());
        for (const auto& [key, text] : file.root["touch"].AsObject()) add(id + " touch \"" + key + "\"", text.AsString());
    }
    int checked = 0;
    for (const bool system : {false, true}) {
        Render::FontAtlas fonts;
        fonts.SetSystemFontsEnabled(system);
        if (system && !std::filesystem::exists(Render::FontAtlas::FontsDirectory())) continue;
        for (const char* face : {"Arial Narrow", "Arial", "Arial Black", "Verdana"}) {
            std::map<char32_t, std::string> missing;   // each code point once, with where it is used first
            for (const auto& [where, codePoints] : texts) {
                for (const char32_t codePoint : codePoints) {
                    if (codePoint < 0x20) continue;
                    ++checked;
                    if (fonts.RoutedGlyph(face, codePoint) == 0 && missing.count(codePoint) == 0) missing[codePoint] = where;
                }
            }
            for (const auto& [codePoint, where] : missing) {
                char hex[16];
                std::snprintf(hex, sizeof(hex), "U+%04X", static_cast<unsigned>(codePoint));
                CHECK_MSG(false, std::string(face) + (system ? " (system)" : " (stand-in)") + ": " + hex + " is not in " +
                                     fonts.RoutedFile(face, codePoint) + " (" + where + ")");
            }
        }
    }
    CHECK(checked > 0);
    // The routing itself.
    using Script = Render::FontAtlas::Script;
    CHECK(Render::FontAtlas::ScriptOf(U'\u65E5') == Script::Japanese);   // a kanji
    CHECK(Render::FontAtlas::ScriptOf(U'\u3042') == Script::Japanese);   // hiragana
    CHECK(Render::FontAtlas::ScriptOf(U'\u30FC') == Script::Japanese);   // the katakana long vowel
    CHECK(Render::FontAtlas::ScriptOf(U'\u3002') == Script::Japanese);   // the ideographic full stop
    CHECK(Render::FontAtlas::ScriptOf(U'\uFF01') == Script::Japanese);   // a full-width '!'
    CHECK(Render::FontAtlas::ScriptOf(U'\u0627') == Script::Arabic);
    CHECK(Render::FontAtlas::ScriptOf(U'\uFEFB') == Script::Arabic);     // lam-alef
    CHECK(Render::FontAtlas::ScriptOf(U'\u0416') == Script::Face);       // Cyrillic: the face's own
    CHECK(Render::FontAtlas::ScriptOf(U'\u011F') == Script::Face);       // Turkish g-breve
    CHECK(Render::FontAtlas::ScriptOf(U'\u2019') == Script::Face);
}

// E24: Arabic, logical in, drawing order out (left to right), worked by hand
// from Unicode's ArabicShaping.txt and UAX #9.
void TestArabicShaping() {
    using Render::ArabicShaping::Shape;
    using Render::ArabicShaping::Visual;
    using Render::ArabicShaping::VisualLine;
    // Beh yeh teh: initial, medial, final.
    CHECK(Shape(U"\u0628\u064A\u062A") == U"\uFE91\uFEF4\uFE96");
    // A letter alone, and one that joins nothing after it (alef): isolated.
    CHECK(Shape(U"\u0628") == U"\uFE8F");
    CHECK(Shape(U"\u0627\u0628") == U"\uFE8D\uFE8F");
    // Seen lam alef meem: lam-alef final after the seen; meem isolated after it.
    CHECK(Shape(U"\u0633\u0644\u0627\u0645") == U"\uFEB3\uFEFC\uFEE1");
    // Lam-alef alone, and each alef it takes.
    CHECK(Shape(U"\u0644\u0627") == U"\uFEFB");
    CHECK(Shape(U"\u0644\u0622") == U"\uFEF5");
    CHECK(Shape(U"\u0644\u0623") == U"\uFEF7");
    CHECK(Shape(U"\u0644\u0625") == U"\uFEF9");
    // Harakat are transparent: the letters around one still join.
    CHECK(Shape(U"\u0628\u064E\u062A") == U"\uFE91\u064E\uFE96");
    // Tatweel joins both ways.
    CHECK(Shape(U"\u0628\u0640\u062A") == U"\uFE91\u0640\uFE96");
    // Teh marbuta, final; hamza never joins (and keeps its own code point).
    CHECK(Shape(U"\u0644\u0639\u0628\u0629") == U"\uFEDF\uFECC\uFE92\uFE94");
    CHECK(Shape(U"\u0628\u0621\u0628") == U"\uFE8F\u0621\uFE8F");
    // Latin is left alone.
    CHECK(Shape(U"Alt+Enter") == U"Alt+Enter");

    // One word, right to left: reversed.
    CHECK(VisualLine(U"\u0633\u0644\u0627\u0645", true) == U"\uFEE1\uFEFC\uFEB3");
    // A Latin key name in an Arabic line keeps its order and sits to the left.
    CHECK(VisualLine(U"\u0627\u0636\u063A\u0637 Alt+Enter", true) ==
          U"Alt+Enter \uFEC2\uFED0\uFEBF\uFE8D");
    // A number after Arabic stays a number, in its place.
    CHECK(VisualLine(U"\u0644\u062F\u064A\u0643 25 \u0645\u0627\u0646\u0627", true) ==
          U"\uFE8E\uFEE7\uFE8E\uFEE3 25 \uFEDA\uFEF3\uFEAA\uFEDF");
    // "12:05" is one number.
    CHECK(VisualLine(U"\u0627\u0644\u0648\u0642\u062A 12:05", true) == U"12:05 \uFE96\uFED7\uFEEE\uFEDF\uFE8D");
    // Brackets round Latin after Latin: all left to right, not "(joystick 3(".
    CHECK(VisualLine(U"\u0642\u0641\u0632: CTRL (joystick 3)", true) ==
          U"CTRL (joystick 3) :\uFEB0\uFED4\uFED7");
    // Brackets round Latin after Arabic: mirrored, so they read "( ... )".
    CHECK(VisualLine(U"\u0628 (Supersonic Engine)", true) == U"(Supersonic Engine) \uFE8F");
    // Brackets round Arabic: mirrored.
    CHECK(VisualLine(U"(\u0628)", true) == U"(\uFE8F)");
    // The switch marker stays at the left, in its order.
    CHECK(VisualLine(U"[\u2022] \u0646\u0627\u0641\u0630\u0629", true) == U"[\u2022] \uFE93\uFEAC\uFED3\uFE8E\uFEE7");
    // A line of Latin alone, in an Arabic paragraph: as it reads; UI chrome
    // with no letter in it (the chooser's arrows, an image switch's lone
    // marker, a value) as it is; a Latin line's leading dash, at its right.
    CHECK(VisualLine(U"Ivan Cvetanovi\u0107", true) == U"Ivan Cvetanovi\u0107");
    CHECK(VisualLine(U"[<]", true) == U"[<]");
    CHECK(VisualLine(U"[>]", true) == U"[>]");
    CHECK(VisualLine(U"[\u2022] ", true) == U"[\u2022] ");
    CHECK(VisualLine(U"100%", true) == U"100%");
    CHECK(VisualLine(U"(joystick 3)", true) == U"(joystick 3)");
    CHECK(VisualLine(U" -www.example.com", true) == U"www.example.com- ");
    // In a left-to-right paragraph an Arabic word still reads right to left.
    CHECK(VisualLine(U"\u0627\u0644\u0639\u0631\u0628\u064A\u0629", false) ==
          U"\uFE94\uFEF4\uFE91\uFEAE\uFECC\uFEDF\uFE8D");
    CHECK(VisualLine(U"English", false) == U"English");
    // Every line alone; the breaks where they were.
    CHECK(Visual(U"\u0628\n\u0633\u0644\u0627\u0645", true) == U"\uFE8F\n\uFEE1\uFEFC\uFEB3");

    // A right-to-left block's lines end together: each flush with the widest.
    Render::FontAtlas fonts;
    fonts.SetSystemFontsEnabled(false);
    if (!fonts.FaceFile("Arial Narrow").empty()) {
        const Render::TextLayout block =
            fonts.LayoutCodePoints(U"ab\nabcd", "Arial Narrow", 25.0f, glm::vec2(100.0f, 0.0f), Render::LineAlign::Right);
        const Render::TextLayout shortLine = fonts.LayoutCodePoints(U"ab", "Arial Narrow", 25.0f, glm::vec2(0.0f));
        CHECK_EQ(block.glyphs.size(), std::size_t{6});
        if (block.glyphs.size() == 6) {
            CHECK_NEAR(block.glyphs[2].pen.x, 100.0f);                               // "abcd" from the anchor
            CHECK_NEAR(block.glyphs[0].pen.x, 100.0f + block.width - shortLine.width);   // "ab" pushed right
        }
        const Render::TextLayout left = fonts.LayoutCodePoints(U"ab\nabcd", "Arial Narrow", 25.0f, glm::vec2(100.0f, 0.0f));
        if (left.glyphs.size() == 6) CHECK_NEAR(left.glyphs[0].pen.x, 100.0f);
        // Set in a box (HudCmd::rtlRight): the same block, its widest line
        // ending at the x given - rounded there, where a left anchor truncates.
        const Render::TextLayout boxed = fonts.LayoutCodePoints(U"ab\nabcd", "Arial Narrow", 25.0f,
                                                                glm::vec2(300.0f, 40.0f), Render::LineAlign::RightEdge);
        CHECK_EQ(boxed.glyphs.size(), std::size_t{6});
        CHECK_NEAR(boxed.width, block.width);
        if (boxed.glyphs.size() == 6 && block.glyphs.size() == 6) {
            CHECK_NEAR(boxed.glyphs[2].pen.x, 300.0f - block.width);        // "abcd" ends at 300
            CHECK_NEAR(boxed.glyphs[0].pen.x, 300.0f - shortLine.width);    // "ab" too
            CHECK_NEAR(boxed.glyphs[0].pen.y, block.glyphs[0].pen.y + 40.0f);
        }
        const Render::TextLayout rounded = fonts.LayoutCodePoints(U"ab\nabcd", "Arial Narrow", 25.0f,
                                                                  glm::vec2(299.6f, 40.0f), Render::LineAlign::RightEdge);
        if (rounded.glyphs.size() == 6 && boxed.glyphs.size() == 6) CHECK(rounded.glyphs[2].pen == boxed.glyphs[2].pen);
        // Arabic from the bundled face, in the same atlas as the Latin.
        const Render::TextLayout mixed = fonts.LayoutCodePoints(U"a\uFE8F", "Arial Narrow", 25.0f, glm::vec2(0.0f));
        CHECK_EQ(mixed.glyphs.size(), std::size_t{2});
        if (mixed.glyphs.size() == 2) {
            CHECK_EQ(mixed.glyphs[0].byte, static_cast<unsigned char>('a'));
            CHECK_EQ(mixed.glyphs[1].byte, static_cast<unsigned char>(0));
            CHECK(mixed.glyphs[1].codePoint == U'\uFE8F');
            CHECK_NEAR(mixed.glyphs[1].pen.y, mixed.glyphs[0].pen.y);   // one baseline
        }
    }
}

// E24: a frame's texts grow an atlas once. HudRenderer hands every text's
// characters to FontAtlas::Prepare before it lays any out, so the first frame
// of a Japanese or Arabic screen - a handful of texts, each bringing characters
// the atlas lacks - bakes and uploads the atlas once, not once a text; and
// Portuguese lands on exactly the texels a text laid out alone has.
void TestAtlasBatching() {
    Render::Localization loc;
    CHECK(loc.Load());
    entt::registry registry;
    Render::TextureCache textures(kApp);
    Render::View view;
    view.logicalScreen = glm::vec2(1024.0f, 768.0f);
    view.windowPixels = glm::uvec2(1024, 768);
    view.scale = 1.0f;
    view.viewportMin = glm::vec2(0.0f);
    view.viewportMax = glm::vec2(1024.0f, 768.0f);
    // The main menu's panel titles: one face, one size, one atlas.
    const char* const titles[] = {"Novo jogo", "Cr\xE9" "ditos", "Configura\xE7\xF5" "es", "Sair do jogo", "Como Jogar",
                                  "Melhores tempos", "Jogador versus Jogador"};
    Eth::RenderSnapshot snapshot;
    float y = 20.0f;
    for (const char* title : titles) {
        Eth::HudCmd text;
        text.kind = Eth::HudCmd::Kind::Text;
        text.text = title;
        text.font = "Arial Narrow";
        text.fontSize = 40.0f;
        text.pos = glm::vec2(642.0f, y);
        text.color = 0xFFCBCBE4u;
        snapshot.hud.push_back(text);
        y += 50.0f;
    }

    Render::FontAtlas fonts;
    fonts.SetSystemFontsEnabled(false);
    Render::HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);
    std::vector<Supersonic::ScreenOverlay::Quad> quads;
    loc.SetLanguage(Language::Japanese);
    hud.Build(snapshot, view, quads);
    CHECK(!quads.empty());
    CHECK_EQ(fonts.AtlasCount(), std::size_t{1});
    CHECK_EQ(fonts.BakeCount(), std::uint64_t{1});
    // The next frame: nothing new, nothing baked.
    quads.clear();
    hud.Build(snapshot, view, quads);
    CHECK_EQ(fonts.BakeCount(), std::uint64_t{1});
    // The frame after a switch to Arabic: once more, for all of it.
    loc.SetLanguage(Language::Arabic);
    quads.clear();
    hud.Build(snapshot, view, quads);
    CHECK_EQ(fonts.BakeCount(), std::uint64_t{2});
    hud.Detach();
    // Laid out one by one without Prepare, the same titles bake once each
    // that brings a character the ones before it did not.
    Render::FontAtlas alone;
    alone.SetSystemFontsEnabled(false);
    Render::VisualText visual;
    for (const char* title : titles) {
        alone.LayoutCodePoints(visual.Of(loc.Translate(title, Language::Japanese), false), "Arial Narrow", 40.0f,
                               glm::vec2(0.0f));
    }
    std::printf("  E24 atlas: the menu's %zu titles in Japanese bake once a frame (%llu times laid out one by one)\n",
                std::size(titles), static_cast<unsigned long long>(alone.BakeCount()));
    CHECK(alone.BakeCount() > 1);

    // Portuguese: every quad the glyph a text laid out on its own has, on
    // the same texels of an atlas packed the same way.
    loc.SetLanguage(Language::Portuguese);
    Render::FontAtlas batched;
    batched.SetSystemFontsEnabled(false);
    Render::HudRenderer ptHud;
    ptHud.Attach(registry, textures, batched, loc);
    quads.clear();
    ptHud.Build(snapshot, view, quads);
    CHECK_EQ(batched.BakeCount(), std::uint64_t{1});
    Render::FontAtlas single;
    single.SetSystemFontsEnabled(false);
    std::size_t matched = 0;
    for (const Eth::HudCmd& cmd : snapshot.hud) {
        const Render::TextLayout layout = single.LayoutCp1252(cmd.text, cmd.font, cmd.fontSize, cmd.pos);
        for (const Render::TextGlyph& glyph : layout.glyphs) {
            if (matched >= quads.size()) break;
            const Supersonic::ScreenOverlay::Quad& quad = quads[matched++];
            CHECK(quad.uvMin == glyph.uvMin && quad.uvMax == glyph.uvMax);
            CHECK(quad.min == view.HudToFraction(glyph.min) && quad.max == view.HudToFraction(glyph.max));
        }
    }
    CHECK_EQ(matched, quads.size());   // a 4:3 window: no bars after the text
    ptHud.Detach();
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

bool SameQuads(const std::vector<Supersonic::ScreenOverlay::Quad>& a,
               const std::vector<Supersonic::ScreenOverlay::Quad>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].min != b[i].min || a[i].max != b[i].max || a[i].uvMin != b[i].uvMin || a[i].uvMax != b[i].uvMax ||
            a[i].color != b[i].color || a[i].texture != b[i].texture) {
            return false;
        }
    }
    return true;
}

// E24: showData's panel (menu.as:217) in a right-to-left language. Its title
// and body are two texts at rectPos + (10, y); each carries the panel's right
// edge less the same 10 px (HudCmd::rtlRight, the shadow's offset with it), so
// in Arabic both end at x 1014 - the edge every panel's lines start from, as
// they start from x 642 left to right - and in every other language the mark
// moves nothing. `shown` is the real menu with the cursor on New Game.
void CheckPanelRightToLeft(const Eth::RenderSnapshot& shown) {
    constexpr float kEdge = 1024.0f - 10.0f;
    std::vector<Eth::HudCmd> panel;   // the title and the body, in front of their shadows
    for (const Eth::HudCmd& cmd : shown.hud) {
        if (cmd.kind != Eth::HudCmd::Kind::Text) continue;
        if (cmd.text != "Novo jogo" && cmd.text != Script::novo_jogo) {
            CHECK_MSG(cmd.rtlRight == 0.0f, Eth::Cp1252ToUtf8(cmd.text));   // the Alt+Enter line: not in a box
            continue;
        }
        if (cmd.color == 0xFFCBCBE4u) {
            CHECK_NEAR(cmd.rtlRight, kEdge);
            panel.push_back(cmd);
        } else {
            CHECK_NEAR(cmd.rtlRight, kEdge + cmd.fontSize * 0.1f);
        }
    }
    CHECK_EQ(panel.size(), std::size_t{2});
    if (panel.size() != 2) return;

    entt::registry registry;
    Render::TextureCache textures(kApp);
    Render::FontAtlas fonts;
    fonts.SetSystemFontsEnabled(false);
    Render::Localization loc;
    CHECK(loc.Load());
    Render::HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);
    // 4:3 at one window pixel a logical one: a quad's x times 1024 is its x.
    Render::View view;
    view.logicalScreen = glm::vec2(1024.0f, 768.0f);
    view.windowPixels = glm::uvec2(1024, 768);
    view.scale = 1.0f;
    view.viewportMin = glm::vec2(0.0f);
    view.viewportMax = glm::vec2(1024.0f, 768.0f);
    const auto quadsOf = [&](const Eth::HudCmd& cmd) {
        Eth::RenderSnapshot one;
        one.hud.push_back(cmd);
        std::vector<Supersonic::ScreenOverlay::Quad> quads;
        hud.Build(one, view, quads);
        return quads;
    };
    const auto unmarked = [](Eth::HudCmd cmd) {
        cmd.rtlRight = 0.0f;
        return cmd;
    };

    // Left to right: every quad where it was without the mark, the title
    // from x 642 (+ its first letter's bearing).
    for (const Render::LanguageInfo& info : Render::kLanguages) {
        if (info.rightToLeft) continue;
        loc.SetLanguage(info.language);
        for (const Eth::HudCmd& cmd : panel) {
            const auto marked = quadsOf(cmd);
            CHECK(!marked.empty());
            CHECK_MSG(SameQuads(marked, quadsOf(unmarked(cmd))), std::string(info.id) + " " + Eth::Cp1252ToUtf8(cmd.text));
        }
        const auto title = quadsOf(panel[0]);
        if (!title.empty()) CHECK_MSG(title[0].min.x * 1024.0f >= 642.0f && title[0].min.x * 1024.0f < 648.0f, info.id);
    }

    if (!loc.HasLanguageFile(Language::Arabic)) return;
    loc.SetLanguage(Language::Arabic);
    Render::VisualText visual;
    float ink[2][2] = {};
    for (std::size_t t = 0; t < 2; ++t) {
        const Eth::HudCmd& cmd = panel[t];
        const auto quads = quadsOf(cmd);
        // Exactly the block FontAtlas sets with its widest line ending at the edge.
        const Render::TextLayout expected = fonts.LayoutCodePoints(visual.Of(loc.Translate(cmd.text), true), cmd.font,
                                                                   cmd.fontSize, glm::vec2(kEdge, cmd.pos.y),
                                                                   Render::LineAlign::RightEdge);
        CHECK_EQ(quads.size(), expected.glyphs.size());
        float left = 1e9f;
        float right = -1e9f;
        for (std::size_t i = 0; i < quads.size() && i < expected.glyphs.size(); ++i) {
            CHECK(quads[i].min == view.HudToFraction(expected.glyphs[i].min) &&
                  quads[i].max == view.HudToFraction(expected.glyphs[i].max));
            left = std::min(left, quads[i].min.x * 1024.0f);
            right = std::max(right, quads[i].max.x * 1024.0f);
        }
        float firstPen = 1e9f;
        for (const Render::TextGlyph& glyph : expected.glyphs) firstPen = std::min(firstPen, glyph.pen.x);
        CHECK(firstPen >= kEdge - expected.width - 0.01f);   // nothing starts left of the widest line
        // The ink ends at the edge, give or take the last letter's side bearing,
        // and stays in the panel (from x 632.8).
        CHECK_MSG(right <= kEdge + 3.0f && right >= kEdge - 8.0f, Eth::Cp1252ToUtf8(cmd.text) + ": " + std::to_string(right));
        CHECK(left > 1024.0f - 1024.0f * (1.0f - 0.618f));
        ink[t][0] = left;
        ink[t][1] = right;
    }
    CHECK(std::fabs(ink[0][1] - ink[1][1]) <= 6.0f);   // the title over the body's right edge
    // Unmarked, as before: the title flush left and short of the body's edge.
    const auto before = quadsOf(unmarked(panel[0]));
    float beforeRight = -1e9f;
    for (const auto& quad : before) beforeRight = std::max(beforeRight, quad.max.x * 1024.0f);
    CHECK(beforeRight < ink[1][1] - 50.0f);
    std::printf("  E24 panel ar: title ink x %.0f-%.0f, body %.0f-%.0f, both to x %.0f (unmarked, the title ended at %.0f)\n",
                ink[0][0], ink[0][1], ink[1][0], ink[1][1], kEdge, beforeRight);
    hud.Detach();
}

// ENHANCEMENT E25: showData's panel on a phone's larger menu, in every
// language, at a phone's, a 16:9 screen's and a 16:10 tablet's size, the
// touch wording on (a phone's): every panel the menu and the arena select
// show - title and body composed as menu.cpp composes them, drawn by the
// script's own showData into `machine` (the real menu, booted) - is scaled by
// one factor, at least E1's size on the window and at most kMenuMaxTextGain
// times it, and every glyph of it lies in the box the window shows of the
// panel. The long ones, How to Play and the credits, are set in two columns
// and come out at least kLongPanelGain times E1's size on a phone. With a
// gamepad's icon over the panel (detectJoysticks: at half E1's size, in the
// top corner the language's lines leave free) no glyph is on it. Prints the
// smallest and largest factor over E1's size for each, and the long panels'.
// (Measured on 2400x1080: 1.24 for the Japanese credits, 1.29 for the
// Portuguese How to Play, 1.31 to 1.60 for the rest; their widest lines, one
// in each column, are what holds them.)
constexpr float kLongPanelGain = 1.2f;

void CheckPhonePanels(Eth::Machine& machine) {
    struct Panel {
        std::string title;
        std::string body;
    };
    const std::string noPad = "\xC9 necess\xE1rio ao menos um joystick\n para jogar neste modo.";   // menu.cpp:182
    const std::string onePad = noPad + Script::endl + Script::endl + "J\xE1 h\xE1 um joystick plugado." +
                               Script::endl + "Mude as op\xE7\xF5" "es de entrada no menu" + Script::endl +
                               "de configura\xE7\xF5" "es para poder" + Script::endl +
                               "utilizar o teclado e o joystick" + Script::endl + "por 2 jogadores.";
    std::vector<Panel> panels = {
        {"Cr\xE9" "ditos", Script::creditos + Script::creditosEnhanced},
        {"Melhores tempos", Script::getRecordTimeList()},
        {"Como Jogar", Script::como_jogar},
        {"Jogador versus Jogador", Script::versus},
        {"Jogador versus Jogador", noPad},
        {"Jogador versus Jogador", onePad},
        {"Novo jogo", Script::novo_jogo},
        {"Sair do jogo", ""},
        {"Configura\xE7\xF5" "es", Script::config},
    };
    // The arenas (arena_select.esc's thumbnails), the two with a score locked (menu.cpp:224-231).
    const char* const arenaTitles[] = {"Obelisco", "Inferno", "Cova", "Vale", "Templo Sagrado", "Neblina"};
    for (int n = 1; n <= 6; ++n) {
        std::string extra;
        if (n >= 5) {
            extra = "\n\n\xC9 necess\xE1rio terminar o jogo\nem menos de " + Script::getTimeString(n == 5 ? 720000u : 900000u) +
                    " para\nliberar esta arena.";
        }
        panels.push_back({arenaTitles[n - 1], Script::g_gameData.get("global", "arena" + std::to_string(n)) + extra});
    }

    entt::registry registry;
    Render::TextureCache textures(kApp);
    Render::FontAtlas fonts;
    Render::Localization loc;
    CHECK(loc.Load());
    loc.SetTouch(true);
    Render::HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);
    Eth::InputFrame input;
    input.cursor = input.cursorAbsolute = Eth::vector2(900.0f, 700.0f);   // on no button: the loop draws no panel

    struct Screen {
        glm::uvec2 window;
        Supersonic::SafeAreaInsets safe;
        float longPanels;   // the least How to Play and the credits come out at, times E1's size
    };
    // A 20:9 phone; a notched 19.5:9 one on its side, whose frame is only
    // x1.21 of E1's and whose insets take 72 px of the panel; 16:9 and a
    // 16:10 tablet, whose panels (about 400 px across) take one column.
    const Screen screens[] = {{{2400u, 1080u}, {}, kLongPanelGain},
                              {{2532u, 1170u}, {132.0f, 0.0f, 132.0f, 63.0f}, 1.0f},
                              {{1920u, 1080u}, {}, 1.0f},
                              {{2560u, 1600u}, {}, 1.0f}};
    for (const Screen& screen : screens) {
        const glm::uvec2 window = screen.window;
        const std::string size = std::to_string(window.x) + "x" + std::to_string(window.y);
        const Render::MenuFrame frame = Render::ComputeMenuFrame(window, screen.safe);
        CHECK_MSG(frame.active, size);
        const Render::MenuPanel box = Render::ComputeMenuPanel(frame, window, screen.safe);
        // The first gamepad's icon as detectJoysticks puts it on a phone's
        // panel: at half E1's size, in the top corner the language's lines
        // leave free.
        const glm::vec2 iconSize = glm::vec2(180.0f, 130.0f) * box.minScale * 0.5f;
        const glm::vec2 iconLeft(box.min.x, box.shownMin.y);
        const glm::vec2 iconRight(box.max.x - iconSize.x, box.shownMin.y);
        // Each panel's four texts (the title, the body, their shadows), as the
        // script queues them: without an icon, with it at the right, at the left.
        const auto draw = [&](const bool rightToLeft, const glm::vec2* icon) {
            Script::g_phonePanel = Script::PhonePanel{true,         box.min,      box.max,        box.minScale,
                                                      box.maxScale, box.shownMin, box.shownMax, rightToLeft};
            std::vector<std::vector<Eth::HudCmd>> drawn;
            for (const Panel& panel : panels) {
                Script::g_joystickIconsMin = icon != nullptr ? *icon : glm::vec2(0.0f);
                Script::g_joystickIconsMax = icon != nullptr ? *icon + iconSize : glm::vec2(0.0f);
                Script::showData(panel.title, panel.body);
                machine.Frame(input);
                std::vector<Eth::HudCmd> texts;
                for (const Eth::HudCmd& cmd : machine.Snapshot().hud) {
                    if (cmd.kind == Eth::HudCmd::Kind::Text && cmd.fit.group != 0) texts.push_back(cmd);
                }
                CHECK_EQ(texts.size(), std::size_t{4});
                drawn.push_back(std::move(texts));
            }
            Script::g_joystickIconsMin = Script::g_joystickIconsMax = glm::vec2(0.0f);
            return drawn;
        };
        const std::vector<std::vector<Eth::HudCmd>> plain = draw(false, nullptr);
        const std::vector<std::vector<Eth::HudCmd>> padRight = draw(false, &iconRight);
        const std::vector<std::vector<Eth::HudCmd>> padLeft = draw(true, &iconLeft);
        Eth::RenderSnapshot menu = machine.Snapshot();
        const Render::View view = Render::CameraRig::ComputeView(menu, window, true, &frame);
        CHECK_MSG(view.scale == frame.scale, size);

        for (const Language language : MeasuredLanguages(loc)) {
          loc.SetLanguage(language);
          const char* id = Render::LanguageId(language);
          for (int pass = 0; pass < 2; ++pass) {
            const bool withPad = pass == 1;
            const std::vector<std::vector<Eth::HudCmd>>& drawn =
                !withPad ? plain : (loc.RightToLeft() ? padLeft : padRight);
            const glm::vec2 icon = loc.RightToLeft() ? iconLeft : iconRight;
            float least = 1e9f;
            float most = 0.0f;
            float howTo = 0.0f;
            float credits = 0.0f;
            int howToBreak = -1;
            int creditsBreak = -1;
            for (std::size_t p = 0; p < drawn.size(); ++p) {
                Eth::RenderSnapshot one;
                one.hud = drawn[p];
                std::vector<Supersonic::ScreenOverlay::Quad> quads;
                hud.Build(one, view, quads);
                const float f = hud.FitScale(1);
                const std::string what = size + " " + id + (withPad ? " (a gamepad)" : "") + " \"" +
                                         Eth::Cp1252ToUtf8(panels[p].title) + "\"";
                CHECK_MSG(f >= box.minScale - 1e-5f && f <= box.maxScale + 1e-5f, what + ": " + std::to_string(f));
                least = std::min(least, f / box.minScale);
                most = std::max(most, f / box.minScale);
                if (p == 0) {
                    credits = f / box.minScale;
                    creditsBreak = hud.FitColumnBreak(1);
                } else if (p == 2) {
                    howTo = f / box.minScale;
                    howToBreak = hud.FitColumnBreak(1);
                }
                // No glyph on the gamepad's icon.
                if (withPad) {
                    for (const Supersonic::ScreenOverlay::Quad& quad : quads) {
                        if (quad.texture.empty()) continue;
                        const glm::vec2 min = (quad.min * glm::vec2(window) - view.viewportMin) / view.scale;
                        const glm::vec2 max = (quad.max * glm::vec2(window) - view.viewportMin) / view.scale;
                        if (min.x < icon.x + iconSize.x && max.x > icon.x && min.y < icon.y + iconSize.y &&
                            max.y > icon.y) {
                            CHECK_MSG(false, what + ": a glyph on the gamepad's icon at (" + std::to_string(min.x) +
                                                 ", " + std::to_string(min.y) + ")");
                            break;
                        }
                    }
                }
                // Every glyph in the box: window fractions back to logical px.
                for (const Supersonic::ScreenOverlay::Quad& quad : quads) {
                    if (quad.texture.empty()) continue;   // the bars (none here)
                    const glm::vec2 min = (quad.min * glm::vec2(window) - view.viewportMin) / view.scale;
                    const glm::vec2 max = (quad.max * glm::vec2(window) - view.viewportMin) / view.scale;
                    // Right to left, the last letter's ink may pass the edge its
                    // advance ends at by its side bearing, as E24's panel allows
                    // (3 px at 25 px): within the panel's 10 px margin.
                    // And the last line's Arabic letters (Noto Sans Arabic)
                    // reach below the Arial line box the fit counts, by as much.
                    const float rightSlack = loc.RightToLeft() ? 0.12f * 25.0f * f + 0.5f : 2.0f;
                    const float bottomSlack = loc.RightToLeft() ? 0.12f * 25.0f * f + 0.5f : 2.0f;
                    const bool inside = min.x >= box.min.x - 2.0f && max.x <= box.max.x + rightSlack &&
                                        min.y >= box.min.y - 4.0f && max.y <= box.max.y + bottomSlack;
                    if (!inside) {
                        CHECK_MSG(false, what + ": a glyph at (" + std::to_string(min.x) + ", " + std::to_string(min.y) +
                                             ")-(" + std::to_string(max.x) + ", " + std::to_string(max.y) +
                                             ") past the box (" + std::to_string(box.min.x) + ", " +
                                             std::to_string(box.min.y) + ")-(" + std::to_string(box.max.x) + ", " +
                                             std::to_string(box.max.y) + ") at " + std::to_string(f / box.minScale));
                        break;
                    }
                }
            }
            std::printf("  E25 panels %s %s%s: %.2f to %.2f times E1's size; How to Play %.2f (%s), the credits "
                        "%.2f (%s)\n",
                        size.c_str(), id, withPad ? " with a gamepad" : "", least, most, howTo,
                        howToBreak >= 0 ? ("2 columns from line " + std::to_string(howToBreak + 1)).c_str() : "1 column",
                        credits,
                        creditsBreak >= 0 ? ("2 columns from line " + std::to_string(creditsBreak + 1)).c_str()
                                          : "1 column");
            // The long panels clearly larger on a 20:9 phone, a gamepad's icon or not.
            CHECK_MSG(howTo >= screen.longPanels - 1e-3f, size + " " + id + ": How to Play at " + std::to_string(howTo));
            CHECK_MSG(credits >= screen.longPanels - 1e-3f, size + " " + id + ": the credits at " + std::to_string(credits));
          }
        }
    }
    loc.SetLanguage(Language::English);
    Script::g_phonePanel = Script::PhonePanel{};
    hud.Detach();
}

// E25's two columns (HudRenderer, Eth::TextFit::columns), on a text of our
// own: twelve lines of W, a blank line, twelve of I, in a box too short for
// one column. It is broken at the blank line, both columns at one factor, the
// second a gap to the right of the first's widest line - to its left for a
// right-to-left language, which reads it first from the right. A text with
// no blank line, or one that fits whole, stays one column. With an avoid box
// at the top right no glyph is in it.
void TestFitColumns() {
    entt::registry registry;
    Render::TextureCache textures(kApp);
    Render::FontAtlas fonts;
    Render::Localization loc;
    CHECK(loc.Load());
    Render::HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);
    Render::View view;
    view.logicalScreen = glm::vec2(1024.0f, 768.0f);
    view.windowPixels = glm::uvec2(1024, 768);
    view.scale = 1.0f;
    view.viewportMin = glm::vec2(0.0f);
    view.viewportMax = glm::vec2(1024.0f, 768.0f);

    std::string text;
    for (int i = 0; i < 12; ++i) text += "WWW\n";
    text += "\n";
    for (int i = 0; i < 12; ++i) text += (i + 1 < 12) ? "II\n" : "II";
    const auto build = [&](const std::string& body, const Eth::uint columns, const glm::vec2& avoidMin,
                           const glm::vec2& avoidMax, std::vector<Supersonic::ScreenOverlay::Quad>& quads) {
        Eth::HudCmd cmd;
        cmd.kind = Eth::HudCmd::Kind::Text;
        cmd.text = body;
        cmd.font = "Arial Narrow";
        cmd.fontSize = 20.0f;
        cmd.color = 0xFFFFFFFFu;
        cmd.pos = glm::vec2(110.0f, 110.0f);
        cmd.rtlRight = 690.0f;
        cmd.fit.min = glm::vec2(100.0f, 100.0f);
        cmd.fit.max = glm::vec2(700.0f, 420.0f);
        cmd.fit.group = 1;
        cmd.fit.minScale = 1.0f;
        cmd.fit.maxScale = 2.0f;
        cmd.fit.columns = columns;
        cmd.fit.avoidMin = avoidMin;
        cmd.fit.avoidMax = avoidMax;
        Eth::RenderSnapshot one;
        one.hud.push_back(cmd);
        quads.clear();
        hud.Build(one, view, quads);
    };
    const auto logical = [&](const Supersonic::ScreenOverlay::Quad& quad) {
        return std::make_pair(quad.min * glm::vec2(1024.0f, 768.0f), quad.max * glm::vec2(1024.0f, 768.0f));
    };
    std::vector<Supersonic::ScreenOverlay::Quad> quads;
    for (const Language language : {Language::English, Language::Arabic}) {
        loc.SetLanguage(language);
        const bool rtl = loc.RightToLeft();
        const std::string what = Render::LanguageId(language);
        build(text, 2, glm::vec2(0.0f), glm::vec2(0.0f), quads);
        CHECK_MSG(hud.FitColumnBreak(1) == 12, what + ": broken at line " + std::to_string(hud.FitColumnBreak(1)));
        CHECK_MSG(hud.FitScale(1) > 1.0f, what);
        CHECK_EQ(quads.size(), std::size_t{12 * 3 + 12 * 2});
        if (quads.size() != std::size_t{60}) continue;
        // The first 36 quads are the first column's Ws, the rest the second's Is.
        float firstMin = 1e9f;
        float firstMax = -1e9f;
        float secondMin = 1e9f;
        float secondMax = -1e9f;
        float bottom = 0.0f;
        for (std::size_t i = 0; i < quads.size(); ++i) {
            const auto [min, max] = logical(quads[i]);
            bottom = std::max(bottom, max.y);
            if (i < 36) {
                firstMin = std::min(firstMin, min.x);
                firstMax = std::max(firstMax, max.x);
            } else {
                secondMin = std::min(secondMin, min.x);
                secondMax = std::max(secondMax, max.x);
            }
        }
        std::printf("  E25 columns %s: x%.2f, the first at x %.0f-%.0f, the second at x %.0f-%.0f, down to y %.0f\n",
                    what.c_str(), hud.FitScale(1), firstMin, firstMax, secondMin, secondMax, bottom);
        if (rtl) CHECK_MSG(secondMax < firstMin, what);   // read from the right: the first column is the right one
        else CHECK_MSG(secondMin > firstMax, what);
        CHECK_MSG(bottom <= 420.0f + 1.0f && std::min(firstMin, secondMin) >= 100.0f - 1.0f &&
                      std::max(firstMax, secondMax) <= 700.0f + 3.0f,
                  what);
        // An avoid box over the top of the second column's outer edge, in the
        // corner the lines leave free: the group a little smaller, clear of it.
        const float free = hud.FitScale(1);
        const glm::vec2 avoidMin = rtl ? glm::vec2(100.0f, 100.0f) : glm::vec2(secondMax - 5.0f, 100.0f);
        const glm::vec2 avoidMax = rtl ? glm::vec2(secondMin + 5.0f, 150.0f) : glm::vec2(700.0f, 150.0f);
        build(text, 2, avoidMin, avoidMax, quads);
        std::printf("  E25 columns %s with an avoid box: x%.2f\n", what.c_str(), hud.FitScale(1));
        CHECK_MSG(hud.FitScale(1) < free && hud.FitScale(1) > 1.0f, what);
        CHECK_MSG(hud.FitColumnBreak(1) == 12, what);
        for (const Supersonic::ScreenOverlay::Quad& quad : quads) {
            const auto [min, max] = logical(quad);
            CHECK_MSG(!(min.x < avoidMax.x - 1.0f && max.x > avoidMin.x + 1.0f && min.y < avoidMax.y - 1.0f &&
                        max.y > avoidMin.y + 1.0f),
                      what + ": a glyph in the avoid box");
        }
        // One column where the text has no blank line, or fits whole, or may not take two.
        std::string noBlank = text;
        noBlank.erase(noBlank.find("\n\n"), 1);
        build(noBlank, 2, glm::vec2(0.0f), glm::vec2(0.0f), quads);
        CHECK_EQ(hud.FitColumnBreak(1), -1);
        build(text, 1, glm::vec2(0.0f), glm::vec2(0.0f), quads);
        CHECK_EQ(hud.FitColumnBreak(1), -1);
        CHECK_NEAR(hud.FitScale(1), 1.0f);   // too tall, kept at its least
        build("WWW\n\nII", 2, glm::vec2(0.0f), glm::vec2(0.0f), quads);
        CHECK_EQ(hud.FitColumnBreak(1), -1);
        CHECK_NEAR(hud.FitScale(1), 2.0f);
    }
    loc.SetLanguage(Language::English);
    hud.Detach();
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
    CheckPanelRightToLeft(shown);   // E24
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

    machine.SetSidesShown(true);
    CheckPhonePanels(machine);   // E25
}

} // namespace

// E26's DrawSpritePart, through the runtime and the renderer: a part of a
// sprite keeps its rectangle (clamped to the image) and draws at the part's
// own size, a part with nothing of the image in it draws nothing, and every
// other sprite command is what it was - the whole image.
void TestSpritePart() {
    Eth::MachineConfig config;
    config.userRoot.clear();
    config.screenSize = Eth::vector2(1024.0f, 768.0f);
    Eth::Machine machine(config);
    Eth::Machine::Scope scope(machine);
    machine.RegisterFunction("pre", [] {});
    machine.RegisterFunction("loop", [] {
        Eth::LoadSprite("interface/frame.png");   // 226 x 74
        Eth::DrawSprite("interface/frame.png", Eth::vector2(4.0f, 5.0f), 0xA0FFFFFFu);
        Eth::DrawSpritePart("interface/frame.png", Eth::vector2(10.0f, 20.0f), Eth::vector2(176.0f, 47.0f),
                            Eth::vector2(192.0f, 63.0f), 0xA0FFFFFFu);
        // Partly past the image's edges: what is inside them.
        Eth::DrawSpritePart("interface/frame.png", Eth::vector2(30.0f, 40.0f), Eth::vector2(200.0f, -5.0f),
                            Eth::vector2(300.0f, 10.0f), 0xFFFFFFFFu);
        // Wholly past them: nothing.
        Eth::DrawSpritePart("interface/frame.png", Eth::vector2(0.0f), Eth::vector2(300.0f, 0.0f),
                            Eth::vector2(310.0f, 10.0f), 0xFFFFFFFFu);
        Eth::DrawShapedSprite("interface/frame.png", Eth::vector2(60.0f, 70.0f), Eth::vector2(50.0f, 30.0f),
                              0xFFFFFFFFu);
    });
    machine.Boot([] { Eth::LoadScene("", "pre", "loop"); });
    for (int i = 0; i < 3; ++i) machine.Frame(Eth::InputFrame{});

    const Eth::RenderSnapshot snapshot = machine.Snapshot();
    CHECK_EQ(snapshot.hud.size(), std::size_t{4});
    if (snapshot.hud.size() != 4u) return;
    const Eth::HudCmd& whole = snapshot.hud[0];
    const Eth::HudCmd& part = snapshot.hud[1];
    const Eth::HudCmd& clamped = snapshot.hud[2];
    const Eth::HudCmd& shaped = snapshot.hud[3];
    // The plain sprite: the whole image at bitmap size, as 0.7.12 drew it.
    CHECK(whole.kind == Eth::HudCmd::Kind::Sprite);
    CHECK(whole.pos == Eth::vector2(4.0f, 5.0f) && whole.size == Eth::vector2(226.0f, 74.0f));
    CHECK(whole.spriteRectMin == Eth::vector2(0.0f) && whole.spriteRectMax == Eth::vector2(226.0f, 74.0f));
    // The part: its rectangle, its own size, the colour it was given.
    CHECK(part.kind == Eth::HudCmd::Kind::Sprite && part.sprite == whole.sprite);
    CHECK(part.pos == Eth::vector2(10.0f, 20.0f) && part.size == Eth::vector2(16.0f, 16.0f));
    CHECK(part.spriteRectMin == Eth::vector2(176.0f, 47.0f) && part.spriteRectMax == Eth::vector2(192.0f, 63.0f));
    CHECK_EQ(part.color, 0xA0FFFFFFu);
    CHECK(clamped.spriteRectMin == Eth::vector2(200.0f, 0.0f) && clamped.spriteRectMax == Eth::vector2(226.0f, 10.0f));
    CHECK(clamped.size == Eth::vector2(26.0f, 10.0f));
    // The stretched sprite is still the whole image, stretched.
    CHECK(shaped.kind == Eth::HudCmd::Kind::ShapedSprite && shaped.size == Eth::vector2(50.0f, 30.0f));
    CHECK(shaped.spriteRectMin == Eth::vector2(0.0f) && shaped.spriteRectMax == Eth::vector2(226.0f, 74.0f));

    // Drawn: the quad's texture coordinates are the rectangle's share of the image.
    entt::registry registry;
    Render::TextureCache textures(kApp);
    Render::FontAtlas fonts;
    Render::Localization loc;
    loc.Load();
    Render::HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);
    Render::View view;
    view.logicalScreen = glm::vec2(1024.0f, 768.0f);
    view.windowPixels = glm::uvec2(1024, 768);
    view.scale = 1.0f;
    view.viewportMin = glm::vec2(0.0f);
    view.viewportMax = glm::vec2(1024.0f, 768.0f);
    std::vector<Supersonic::ScreenOverlay::Quad> quads;
    hud.Build(snapshot, view, quads);
    CHECK_EQ(quads.size(), std::size_t{4});
    if (quads.size() == 4u) {
        CHECK_NEAR(quads[0].uvMin.x, 0.0f);
        CHECK_NEAR(quads[0].uvMax.x, 1.0f);
        CHECK_NEAR(quads[0].uvMax.y, 1.0f);
        CHECK_NEAR(quads[1].uvMin.x, 176.0f / 226.0f);
        CHECK_NEAR(quads[1].uvMin.y, 47.0f / 74.0f);
        CHECK_NEAR(quads[1].uvMax.x, 192.0f / 226.0f);
        CHECK_NEAR(quads[1].uvMax.y, 63.0f / 74.0f);
        // At the part's own size: 16 px of the 1024 x 768 screen.
        CHECK_NEAR(quads[1].min.x, 10.0f / 1024.0f);
        CHECK_NEAR(quads[1].max.x, 26.0f / 1024.0f);
        CHECK_NEAR(quads[1].min.y, 20.0f / 768.0f);
        CHECK_NEAR(quads[1].max.y, 36.0f / 768.0f);
        CHECK_NEAR(quads[2].uvMin.x, 200.0f / 226.0f);
        CHECK_NEAR(quads[2].uvMax.x, 1.0f);
        CHECK_NEAR(quads[2].uvMin.y, 0.0f);
        CHECK_NEAR(quads[2].uvMax.y, 10.0f / 74.0f);
        // The stretched sprite takes the whole image.
        CHECK_NEAR(quads[3].uvMin.x, 0.0f);
        CHECK_NEAR(quads[3].uvMax.y, 1.0f);
    }
    hud.Detach();
}

// E26's plaque through the real game: level 1 on a 2400x1080 phone, the HUD's
// commands drawn by the ported scripts. Without the plaque (the desktop, or a
// frame alone) the panel is the original's: one frame.png, whole, at the
// bars. With it the bars start 16 px inside the plaque's corner, and frame.png
// has the stone it lacks on its top and left, as parts of itself - for the
// first player a whole strip at the left, for the second the 10 px between the
// first's right strip and its bars - none overlapping another or the frame's
// own stone (the frame is drawn translucent: it would show twice).
struct PanelDrawn {
    std::vector<Eth::HudCmd> frames;    // interface/frame.png, whole
    std::vector<Eth::HudCmd> parts;     // parts of it
    std::vector<Eth::HudCmd> rails;     // interface/rail.png, one per bar
    std::vector<Eth::HudCmd> hp;        // interface/hp.png
    std::vector<Eth::HudCmd> skulls;    // interface/skull_interface.png
    std::vector<Eth::HudCmd> values;    // "hp: ", "mp: ", "lv: "
    std::vector<Eth::HudCmd> messages;  // Arial 30
};

PanelDrawn SortPanel(const std::vector<Eth::HudCmd>& hud) {
    PanelDrawn out;
    for (const Eth::HudCmd& cmd : hud) {
        const auto named = [&cmd](const char* file) {
            return cmd.kind != Eth::HudCmd::Kind::Text && cmd.sprite.find(file) != std::string::npos;
        };
        if (named("frame.png")) {
            const bool whole = cmd.spriteRectMin == glm::vec2(0.0f) && cmd.spriteRectMax == glm::vec2(226.0f, 74.0f);
            (whole ? out.frames : out.parts).push_back(cmd);
        } else if (named("rail.png")) {
            out.rails.push_back(cmd);
        } else if (named("/hp.png")) {
            out.hp.push_back(cmd);
        } else if (named("skull_interface.png")) {
            out.skulls.push_back(cmd);
        } else if (cmd.kind == Eth::HudCmd::Kind::Text) {
            if (cmd.text.rfind("hp: ", 0) == 0 || cmd.text.rfind("mp: ", 0) == 0 || cmd.text.rfind("lv: ", 0) == 0) {
                out.values.push_back(cmd);
            } else if (cmd.font == "Arial" && cmd.fontSize == 30.0f) {
                out.messages.push_back(cmd);
            }
        }
    }
    return out;
}

// A destination rectangle and the part of frame.png it shows, for one panel
// whose bars start at `bars`; `player` 0 is the first.
struct Piece {
    glm::vec2 dest;
    glm::vec2 srcMin;
    glm::vec2 srcMax;
};

std::vector<Piece> PlaquePieces(const glm::vec2& bars, const int player) {
    const glm::vec2 o = bars + glm::vec2(226.0f * static_cast<float>(player), 0.0f);
    const float left = player == 0 ? 16.0f : 10.0f;   // 226 - (200 + 16)
    return {
        {o + glm::vec2(0.0f, -16.0f), {0.0f, 47.0f}, {200.0f, 63.0f}},                           // the top strip
        {o + glm::vec2(200.0f, -16.0f), {176.0f, 47.0f}, {192.0f, 63.0f}},                       // its right corner
        {o + glm::vec2(216.0f, -16.0f), {216.0f, 0.0f}, {222.0f, 16.0f}},                        // that corner's shadow
        {o + glm::vec2(-left, -16.0f), {48.0f - left, 47.0f}, {48.0f, 63.0f}},                   // the left top corner
        {o + glm::vec2(-left, 0.0f), {200.0f, 0.0f}, {200.0f + left, 47.0f}},                    // the left side
        {o + glm::vec2(-left, 47.0f), {112.0f - left, 47.0f}, {112.0f, 67.0f}},                  // the left bottom corner
    };
}

bool SameRect(const Eth::HudCmd& cmd, const Piece& piece) {
    return Near(cmd.pos.x, piece.dest.x) && Near(cmd.pos.y, piece.dest.y) &&
           cmd.spriteRectMin == piece.srcMin && cmd.spriteRectMax == piece.srcMax &&
           cmd.size == piece.srcMax - piece.srcMin && cmd.color == 0xA0FFFFFFu;
}

bool Overlaps(const glm::vec2& aMin, const glm::vec2& aMax, const glm::vec2& bMin, const glm::vec2& bMax) {
    return aMin.x < bMax.x - 0.001f && bMin.x < aMax.x - 0.001f && aMin.y < bMax.y - 0.001f &&
           bMin.y < aMax.y - 0.001f;
}

template <typename Run>
void InLevelOneForPlaque(const char* what, Run run) {
    namespace fs = std::filesystem;
    if (!fs::exists(fs::path(PENUMBRA_ORIGINAL_DIR) / "scenes" / "level1.esc")) {
        std::printf("  (the original is not at %s: %s is skipped)\n", PENUMBRA_ORIGINAL_DIR, what);
        return;
    }
    std::error_code ec;
    const fs::path userRoot = fs::temp_directory_path(ec) / ("penumbra-plaque-" + std::to_string(std::random_device{}()));
    fs::remove_all(userRoot, ec);
    fs::create_directories(userRoot, ec);
    {
        const glm::uvec2 phone(2400u, 1080u);
        const Eth::vector2 levelScreen =
            Render::ZoomedScreen(phone, true, Render::CampaignZoom(0, true, phone, true));
        Eth::MachineConfig config;
        config.userRoot = userRoot.generic_string();
        config.screenSizeForScene = [&](const std::string& scene) {
            return Render::IsCampaignScene(scene) ? levelScreen : Eth::vector2(1024.0f, 768.0f);
        };
        Eth::Machine machine(config);
        Eth::Machine::Scope scope(machine);
        Script::RegisterAll(machine);
        machine.Boot(Script::ScriptMain);
        machine.Frame(Eth::InputFrame{});
        const Eth::uint setup = Script::g_levelStartTime;
        Script::newGame("CAMPAIGN");
        for (int i = 0; i < 5 && Script::g_levelStartTime == setup; ++i) machine.Frame(Eth::InputFrame{});
        Eth::ETHEntity wizard = Eth::SeekEntity("bruxo.ent");
        const auto standing = [](const Eth::ETHEntity& e) {
            return e != nullptr && e->IsAlive() && e->GetIntData("hp") > 0 &&
                   e->CheckCustomData("deathTime") == Eth::DT_NODATA && e->GetUIntData("touchingGround") != 0;
        };
        for (int i = 0; i < 300 && !standing(wizard); ++i) {
            machine.Frame(Eth::InputFrame{});
            wizard = Eth::SeekEntity("bruxo.ent");
        }
        if (!standing(wizard)) {
            CHECK_MSG(false, std::string("no wizard standing in level 1: ") + what);
        } else {
            run(machine, wizard);
        }
        CHECK_EQ(machine.ScriptAborts(), 0u);
    }
    Script::g_touchHud = Script::TouchHud{};
    fs::remove_all(userRoot, ec);
}

void TestPlaqueInGame() {
    InLevelOneForPlaque("the HUD's plaque", [](Eth::Machine& machine, Eth::ETHEntity wizard) {
        // Two frames each: an entity callback's draws (the bars, from the
        // wizard's) reach the snapshot a frame after the loop's (the timer).
        const auto frameWith = [&](const Script::TouchHud& hud) {
            Script::g_touchHud = hud;
            machine.Frame(Eth::InputFrame{});
            machine.Frame(Eth::InputFrame{});
            return SortPanel(machine.Snapshot().hud);
        };
        Script::g_messages.addMessage("Checkpoint...");

        // Off the plaque: the original's panel, from (0, 0), and E26's frame
        // alone moves it, whole.
        const PanelDrawn plain = frameWith(Script::TouchHud{});
        CHECK_EQ(plain.frames.size(), std::size_t{1});
        CHECK(plain.parts.empty());
        if (plain.frames.size() == 1u) CHECK(plain.frames[0].pos == Eth::vector2(0.0f) && plain.frames[0].color == 0xA0FFFFFFu);
        CHECK_EQ(plain.rails.size(), std::size_t{3});
        CHECK_EQ(plain.skulls.size(), std::size_t{1});
        if (plain.skulls.size() == 1u) CHECK(plain.skulls[0].pos == Eth::vector2(448.0f, 0.0f));
        const PanelDrawn framed = frameWith(Script::TouchHud{true, 40.0f, 18.0f, 30.0f});
        CHECK(framed.parts.empty());
        CHECK_EQ(framed.frames.size(), std::size_t{1});
        if (framed.frames.size() == 1u) CHECK(framed.frames[0].pos == Eth::vector2(40.0f, 18.0f));
        CHECK(Script::hudBarsTopLeft() == Eth::vector2(40.0f, 18.0f));
        float framedMessageTop = 1.0e9f;
        for (const Eth::HudCmd& cmd : framed.messages) framedMessageTop = std::min(framedMessageTop, cmd.pos.y);
        CHECK(!framed.messages.empty());

        // On it: the panel's corner at the safe area's (12, 5), the bars 16 in.
        Script::TouchHud on{true, 40.0f, 18.0f, 30.0f, 0.0f, true, 12.0f, 5.0f};
        const PanelDrawn plaque = frameWith(on);
        const glm::vec2 bars(28.0f, 21.0f);
        CHECK(Script::hudBarsTopLeft() == Eth::vector2(28.0f, 21.0f));
        CHECK_EQ(plaque.frames.size(), std::size_t{1});
        if (plaque.frames.size() == 1u) CHECK(plaque.frames[0].pos == bars);
        for (const Eth::HudCmd& rail : plaque.rails) CHECK(Near(rail.pos.x, bars.x));
        CHECK_EQ(plaque.skulls.size(), std::size_t{1});
        if (plaque.skulls.size() == 1u) CHECK(plaque.skulls[0].pos == glm::vec2(bars.x + 448.0f, bars.y));   // the bars' row
        for (const Eth::HudCmd& cmd : plaque.values) CHECK_MSG(cmd.pos.x >= bars.x + 2.0f - 0.01f, cmd.text);
        // The pieces of the first player's plaque, each drawn once, and nothing else of frame.png.
        const std::vector<Piece> first = PlaquePieces(bars, 0);
        CHECK_EQ(plaque.parts.size(), first.size());
        for (const Piece& piece : first) {
            const bool drawn = std::any_of(plaque.parts.begin(), plaque.parts.end(),
                                           [&](const Eth::HudCmd& cmd) { return SameRect(cmd, piece); });
            CHECK_MSG(drawn, "a plaque piece at " + std::to_string(piece.dest.x) + ", " + std::to_string(piece.dest.y));
        }
        // The message lines start no higher than the plaque's end (its stone and the shadow under it) and 6 px more.
        float plaqueMessageTop = 1.0e9f;
        for (const Eth::HudCmd& cmd : plaque.messages) plaqueMessageTop = std::min(plaqueMessageTop, cmd.pos.y);
        CHECK_NEAR(plaqueMessageTop, std::max(framedMessageTop, 5.0f + 16.0f + 67.0f + 6.0f));
        CHECK(plaque.messages.size() == framed.messages.size());

        // The plaque is the safe area's, whatever the margin: the margin's
        // frame moves the timer, the messages' left and the pause, never the panel.
        Script::TouchHud wide = on;
        wide.left = 90.0f;
        wide.top = 60.0f;
        const PanelDrawn moved = frameWith(wide);
        CHECK_EQ(moved.frames.size(), std::size_t{1});
        if (moved.frames.size() == 1u) CHECK(moved.frames[0].pos == bars);
        float movedMessageTop = 1.0e9f;
        for (const Eth::HudCmd& cmd : moved.messages) movedMessageTop = std::min(movedMessageTop, cmd.pos.y);
        CHECK_NEAR(movedMessageTop, 70.0f + 60.0f);   // already below the plaque

        // A second player's panel (the princess, summoned beside the wizard):
        // its left piece is the gap before its bars, and the stone runs on.
        Eth::ETHEntity princess;
        Eth::AddEntity(Script::MAIN_CHARACTER_ENTITY1, Eth::vector3(wizard->GetPositionXY() + Eth::vector2(48.0f, -6.0f), 0.0f), princess);
        CHECK(princess != nullptr);
        PanelDrawn pair;
        for (int i = 0; i < 4; ++i) pair = frameWith(on);
        CHECK_EQ(pair.frames.size(), std::size_t{2});
        const std::vector<Piece> second = PlaquePieces(bars, 1);
        CHECK_EQ(pair.parts.size(), first.size() + second.size());
        for (const Piece& piece : second) {
            const bool drawn = std::any_of(pair.parts.begin(), pair.parts.end(),
                                           [&](const Eth::HudCmd& cmd) { return SameRect(cmd, piece); });
            CHECK_MSG(drawn, "a second plaque piece at " + std::to_string(piece.dest.x) + ", " + std::to_string(piece.dest.y));
        }
        // The stone never doubles: no stone piece meets another, or either frame's stone (shadow slivers excepted).
        std::vector<std::pair<glm::vec2, glm::vec2>> stone;
        for (const Eth::HudCmd& cmd : pair.parts) {
            if (cmd.spriteRectMin.x >= 216.0f) continue;   // the shadow
            stone.push_back({glm::vec2(cmd.pos.x, cmd.pos.y), glm::vec2(cmd.pos.x, cmd.pos.y) + cmd.size});
        }
        for (const Eth::HudCmd& frame : pair.frames) {
            const glm::vec2 at(frame.pos.x, frame.pos.y);
            stone.push_back({at + glm::vec2(0.0f, 47.0f), at + glm::vec2(216.0f, 63.0f)});   // the bottom strip
            stone.push_back({at + glm::vec2(200.0f, 0.0f), at + glm::vec2(216.0f, 47.0f)});   // the right one
        }
        for (std::size_t a = 0; a < stone.size(); ++a) {
            for (std::size_t b = a + 1; b < stone.size(); ++b) {
                CHECK_MSG(!Overlaps(stone[a].first, stone[a].second, stone[b].first, stone[b].second),
                          "stone pieces " + std::to_string(a) + " and " + std::to_string(b) + " overlap");
            }
        }
    });
}

// E27's DrawShapedSpritePart, through the runtime and the renderer: a part of a
// sprite stretched to the size it was given (its own, for 0), the destination
// cut in the same proportion as a rectangle that runs past the image, a part
// with nothing of the image in it queued as nothing, and DrawSpritePart and
// DrawShapedSprite what they were.
void TestShapedSpritePart() {
    Eth::MachineConfig config;
    config.userRoot.clear();
    config.screenSize = Eth::vector2(1024.0f, 768.0f);
    Eth::Machine machine(config);
    Eth::Machine::Scope scope(machine);
    machine.RegisterFunction("pre", [] {});
    machine.RegisterFunction("loop", [] {
        Eth::LoadSprite("interface/frame.png");   // 226 x 74
        // The left half of the image, stretched to 80 x 40.
        Eth::DrawShapedSpritePart("interface/frame.png", Eth::vector2(10.0f, 20.0f), Eth::vector2(80.0f, 40.0f),
                                  Eth::vector2(0.0f), Eth::vector2(113.0f, 37.0f), 0x80FFFFFFu);
        // 100 x 20 px of rectangle past the image's right and top edges, stretched to 50 x 20: the 26 x 10
        // inside the image is what 13 x 10 of it covers.
        Eth::DrawShapedSpritePart("interface/frame.png", Eth::vector2(30.0f, 40.0f), Eth::vector2(50.0f, 20.0f),
                                  Eth::vector2(200.0f, -10.0f), Eth::vector2(300.0f, 10.0f), 0xFFFFFFFFu);
        // A size of 0 is the part's own.
        Eth::DrawShapedSpritePart("interface/frame.png", Eth::vector2(5.0f, 6.0f), Eth::vector2(0.0f),
                                  Eth::vector2(176.0f, 47.0f), Eth::vector2(192.0f, 63.0f), 0xFFFFFFFFu);
        // Nothing of the image in it, or nothing in it: not drawn, and not the whole image.
        Eth::DrawShapedSpritePart("interface/frame.png", Eth::vector2(0.0f), Eth::vector2(10.0f, 10.0f),
                                  Eth::vector2(300.0f, 0.0f), Eth::vector2(310.0f, 10.0f), 0xFFFFFFFFu);
        Eth::DrawShapedSpritePart("interface/frame.png", Eth::vector2(0.0f), Eth::vector2(10.0f, 10.0f),
                                  Eth::vector2(5.0f, 5.0f), Eth::vector2(5.0f, 9.0f), 0xFFFFFFFFu);
        // What was there before.
        Eth::DrawShapedSprite("interface/frame.png", Eth::vector2(60.0f, 70.0f), Eth::vector2(50.0f, 30.0f),
                              0xFFFFFFFFu);
        Eth::DrawSpritePart("interface/frame.png", Eth::vector2(1.0f, 2.0f), Eth::vector2(176.0f, 47.0f),
                            Eth::vector2(192.0f, 63.0f), 0xFFFFFFFFu);
    });
    machine.Boot([] { Eth::LoadScene("", "pre", "loop"); });
    for (int i = 0; i < 3; ++i) machine.Frame(Eth::InputFrame{});

    const Eth::RenderSnapshot snapshot = machine.Snapshot();
    CHECK_EQ(snapshot.hud.size(), std::size_t{5});
    if (snapshot.hud.size() != 5u) return;
    const Eth::HudCmd& half = snapshot.hud[0];
    const Eth::HudCmd& cut = snapshot.hud[1];
    const Eth::HudCmd& own = snapshot.hud[2];
    const Eth::HudCmd& shaped = snapshot.hud[3];
    const Eth::HudCmd& part = snapshot.hud[4];
    CHECK(half.kind == Eth::HudCmd::Kind::ShapedSprite);
    CHECK(half.pos == Eth::vector2(10.0f, 20.0f) && half.size == Eth::vector2(80.0f, 40.0f));
    CHECK(half.spriteRectMin == Eth::vector2(0.0f) && half.spriteRectMax == Eth::vector2(113.0f, 37.0f));
    CHECK_EQ(half.color, 0x80FFFFFFu);
    // The rectangle is cut to the image, the destination in the same proportion (x 1/2, y 1): the
    // pixels still cover what they covered.
    CHECK(cut.kind == Eth::HudCmd::Kind::ShapedSprite);
    CHECK(cut.spriteRectMin == Eth::vector2(200.0f, 0.0f) && cut.spriteRectMax == Eth::vector2(226.0f, 10.0f));
    CHECK_NEAR(cut.pos.x, 30.0f);
    CHECK_NEAR(cut.pos.y, 50.0f);
    CHECK_NEAR(cut.size.x, 13.0f);
    CHECK_NEAR(cut.size.y, 10.0f);
    CHECK(own.kind == Eth::HudCmd::Kind::ShapedSprite && own.size == Eth::vector2(16.0f, 16.0f));
    // DrawShapedSprite stretches the whole image; DrawSpritePart is the part at its own size.
    CHECK(shaped.kind == Eth::HudCmd::Kind::ShapedSprite && shaped.size == Eth::vector2(50.0f, 30.0f));
    CHECK(shaped.spriteRectMin == Eth::vector2(0.0f) && shaped.spriteRectMax == Eth::vector2(226.0f, 74.0f));
    CHECK(part.kind == Eth::HudCmd::Kind::Sprite && part.size == Eth::vector2(16.0f, 16.0f));

    entt::registry registry;
    Render::TextureCache textures(kApp);
    Render::FontAtlas fonts;
    Render::Localization loc;
    loc.Load();
    Render::HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);
    Render::View view;
    view.logicalScreen = glm::vec2(1024.0f, 768.0f);
    view.windowPixels = glm::uvec2(1024, 768);
    view.scale = 1.0f;
    view.viewportMin = glm::vec2(0.0f);
    view.viewportMax = glm::vec2(1024.0f, 768.0f);
    std::vector<Supersonic::ScreenOverlay::Quad> quads;
    hud.Build(snapshot, view, quads);
    CHECK_EQ(quads.size(), std::size_t{5});
    if (quads.size() == 5u) {
        // The half: its texture coordinates are the left half, its quad the 80 x 40 it was given.
        CHECK_NEAR(quads[0].uvMin.x, 0.0f);
        CHECK_NEAR(quads[0].uvMax.x, 113.0f / 226.0f);
        CHECK_NEAR(quads[0].uvMax.y, 37.0f / 74.0f);
        CHECK_NEAR(quads[0].min.x, 10.0f / 1024.0f);
        CHECK_NEAR(quads[0].max.x, 90.0f / 1024.0f);
        CHECK_NEAR(quads[0].min.y, 20.0f / 768.0f);
        CHECK_NEAR(quads[0].max.y, 60.0f / 768.0f);
        CHECK_NEAR(quads[1].uvMin.x, 200.0f / 226.0f);
        CHECK_NEAR(quads[1].uvMax.x, 1.0f);
        CHECK_NEAR(quads[1].min.x, 30.0f / 1024.0f);
        CHECK_NEAR(quads[1].max.x, 43.0f / 1024.0f);
    }
    hud.Detach();
}

namespace {

// What drawPanel queued for a panel at `pos`, `size` (cmds[first, first + count)): pieces that are
// the cells of a grid and so fill the rectangle with no gap (their areas add up to it) and no
// overlap, in the art as well as on the screen.
void CheckPanelPieces(const std::vector<Eth::HudCmd>& cmds, const std::size_t first, const std::size_t count,
                      const glm::vec2& pos, const glm::vec2& size, const uint32_t color, const std::string& what) {
    CHECK_MSG(first + count <= cmds.size(), what + ": its pieces are all there");
    if (first + count > cmds.size()) return;
    float area = 0.0f;
    for (std::size_t a = 0; a < count; ++a) {
        const Eth::HudCmd& c = cmds[first + a];
        CHECK_MSG(c.kind == Eth::HudCmd::Kind::ShapedSprite && c.color == color, what + ": a stretched part in the colour");
        CHECK_MSG(c.size.x > 0.0f && c.size.y > 0.0f, what + ": no empty piece");
        CHECK_MSG(c.pos.x >= pos.x - 0.001f && c.pos.y >= pos.y - 0.001f &&
                      c.pos.x + c.size.x <= pos.x + size.x + 0.001f && c.pos.y + c.size.y <= pos.y + size.y + 0.001f,
                  what + ": inside the panel");
        area += c.size.x * c.size.y;
        for (std::size_t b = a + 1; b < count; ++b) {
            const Eth::HudCmd& d = cmds[first + b];
            CHECK_MSG(!Overlaps(c.pos, c.pos + c.size, d.pos, d.pos + d.size), what + ": pieces overlap");
            CHECK_MSG(!Overlaps(c.spriteRectMin, c.spriteRectMax, d.spriteRectMin, d.spriteRectMax),
                      what + ": pieces of the art overlap");
        }
    }
    CHECK_MSG(std::fabs(area - size.x * size.y) < 0.01f, what + ": the pieces fill the rectangle (" +
                                                             std::to_string(area) + " of " +
                                                             std::to_string(size.x * size.y) + ")");
}

} // namespace

// E27's helpers for the art the port added (game/script/optionsArt.cpp): the file's path, every
// file loading and none of them named as a file of the original is (sprites are looked up by file
// name), icons at their proportions, panels in nine slices whose pieces fill the rectangle exactly -
// also a panel smaller than two slices - and nothing at all drawn without a data folder.
void TestOptionsArt() {
    const std::string savedDir = Script::g_artDir;
    // The path: absolute, forward slashes, no doubled slash.
    Script::g_artDir = "C:\\x\\data\\";
    CHECK_MSG(Script::optionsArtPath("globe.png") == "C:/x/data/images/options/globe.png", Script::optionsArtPath("globe.png"));
    Script::g_artDir = "/opt/penumbra/data";
    CHECK_MSG(Script::optionsArtPath("panel.png") == "/opt/penumbra/data/images/options/panel.png", Script::optionsArtPath("panel.png"));
    Script::g_artDir.clear();
    CHECK(Script::optionsArtPath("globe.png").empty());

    const std::string dataDir = PENUMBRA_DATA_DIR;
    Script::g_artDir = dataDir;
    Eth::MachineConfig config;
    config.userRoot.clear();
    config.screenSize = Eth::vector2(1024.0f, 768.0f);
    Eth::Machine machine(config);
    Eth::Machine::Scope scope(machine);
    machine.RegisterFunction("pre", [] { Script::loadOptionsArt(); });
    machine.RegisterFunction("loop", [] {
        // 0-8: a big panel; 9-12: one narrower than two slices; 13-16: exactly two slices; 17-25: half-size
        // corners at alpha 128 (the room for a middle and edges, so nine); then two icons.
        Script::drawPanel(Eth::vector2(10.0f, 20.0f), Eth::vector2(300.0f, 140.0f));
        Script::drawPanel(Eth::vector2(0.0f), Eth::vector2(40.0f, 30.0f));
        Script::drawPanel(Eth::vector2(5.0f, 5.0f), Eth::vector2(52.0f, 52.0f));
        Script::drawPanel(Eth::vector2(0.0f), Eth::vector2(200.0f, 100.0f), 128, 13.0f);
        Script::drawOptionsIcon("globe", Eth::vector2(100.0f, 50.0f), 40.0f, 200);
        Script::drawOptionsIcon("pad", Eth::vector2(100.0f, 50.0f), 60.0f);
        Script::drawOptionsIcon("nothing_like_this", Eth::vector2(0.0f), 40.0f);   // not an art file: not drawn
        Script::drawOptionsIcon("globe", Eth::vector2(0.0f), 0.0f);                // no size: not drawn
        Script::drawOptionsIcon("globe", Eth::vector2(0.0f), 40.0f, 0);            // invisible: not drawn
    });
    machine.Boot([] { Eth::LoadScene("", "pre", "loop"); });
    for (int i = 0; i < 3; ++i) machine.Frame(Eth::InputFrame{});

    // Every file loaded, at the size its PNG has; the icons are square but the pad.
    const char* const squares[] = {"panel", "check_on", "check_off", "arrow_left", "arrow_right", "minus", "plus",
                                   "speaker", "music", "globe", "monitor"};   // E29: no "globe_button" (the main menu's language button is gone)
    for (const char* name : Script::kOptionsArt) {
        const std::string path = Script::optionsArtPath(std::string(name) + ".png");
        const Eth::vector2 size = Eth::GetSpriteSize(path);
        const glm::ivec2 probed = Render::ProbeImageSize(path);
        CHECK_MSG(size.x > 0.0f && size.y > 0.0f, std::string(name) + " loaded");
        CHECK_MSG(static_cast<int>(size.x) == probed.x && static_cast<int>(size.y) == probed.y,
                  std::string(name) + " has its PNG's size");
        const bool square = std::find_if(std::begin(squares), std::end(squares), [&](const char* n) {
                                return std::string(n) == name;
                            }) != std::end(squares);
        CHECK_MSG(square == (size.x == size.y), std::string(name) + (square ? " is square" : " is not square"));
    }
    CHECK(Eth::GetSpriteSize(Script::optionsArtPath("panel.png")) == Eth::vector2(92.0f, 92.0f));
    CHECK(Eth::GetSpriteSize(Script::optionsArtPath("pad.png")) == Eth::vector2(96.0f, 47.0f));
    CHECK(Script::kPanelSlice * 2.0f < 92.0f);

    // No file shares a name with one of the original's: a sprite is found by its file name.
    std::set<std::string> originals;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(
             kApp, std::filesystem::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
        std::string name = it->path().filename().string();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        originals.insert(name);
    }
    CHECK(originals.size() > 100u);
    for (const char* name : Script::kOptionsArt) {
        CHECK_MSG(originals.count(std::string(name) + ".png") == 0u,
                  std::string(name) + ".png is also a file of the original");
    }

    const Eth::RenderSnapshot snapshot = machine.Snapshot();
    const std::vector<Eth::HudCmd>& cmds = snapshot.hud;
    CHECK_EQ(cmds.size(), std::size_t{9 + 4 + 4 + 9 + 2});
    if (cmds.size() != 28u) return;
    // The big panel: nine pieces on the grid x 10, 36, 284, 310 and y 20, 46, 134, 160, from the art's
    // 0, 26, 66, 92 on both axes.
    CheckPanelPieces(cmds, 0, 9, glm::vec2(10.0f, 20.0f), glm::vec2(300.0f, 140.0f), 0xFFFFFFFFu, "300x140");
    CHECK(cmds[0].pos == Eth::vector2(10.0f, 20.0f) && cmds[0].size == Eth::vector2(26.0f, 26.0f));
    CHECK(cmds[0].spriteRectMin == Eth::vector2(0.0f) && cmds[0].spriteRectMax == Eth::vector2(26.0f, 26.0f));
    CHECK(cmds[4].pos == Eth::vector2(36.0f, 46.0f) && cmds[4].size == Eth::vector2(248.0f, 88.0f));
    CHECK(cmds[4].spriteRectMin == Eth::vector2(26.0f) && cmds[4].spriteRectMax == Eth::vector2(66.0f));
    CHECK(cmds[8].pos == Eth::vector2(284.0f, 134.0f) && cmds[8].size == Eth::vector2(26.0f, 26.0f));
    CHECK(cmds[8].spriteRectMin == Eth::vector2(66.0f) && cmds[8].spriteRectMax == Eth::vector2(92.0f));
    // Its source pieces are the whole image: no part of the art is left out or drawn twice.
    float sourceArea = 0.0f;
    for (std::size_t i = 0; i < 9; ++i) {
        sourceArea += (cmds[i].spriteRectMax.x - cmds[i].spriteRectMin.x) *
                      (cmds[i].spriteRectMax.y - cmds[i].spriteRectMin.y);
    }
    CHECK_NEAR(sourceArea, 92.0f * 92.0f);
    // Narrower than two slices: corners of 20 x 15, still cut from the art's 26 x 26, and no middle.
    CheckPanelPieces(cmds, 9, 4, glm::vec2(0.0f), glm::vec2(40.0f, 30.0f), 0xFFFFFFFFu, "40x30");
    CHECK(cmds[9].size == Eth::vector2(20.0f, 15.0f) && cmds[9].spriteRectMax == Eth::vector2(26.0f, 26.0f));
    // Exactly two slices: four corners.
    CheckPanelPieces(cmds, 13, 4, glm::vec2(5.0f, 5.0f), glm::vec2(52.0f, 52.0f), 0xFFFFFFFFu, "52x52");
    // Corners drawn 13 px from the art's 26, at alpha 128: 200 x 100 has room for a middle and edges.
    CheckPanelPieces(cmds, 17, 9, glm::vec2(0.0f), glm::vec2(200.0f, 100.0f), 0x80FFFFFFu, "200x100 at 13");
    CHECK(cmds[17].size == Eth::vector2(13.0f, 13.0f) && cmds[17].spriteRectMax == Eth::vector2(26.0f, 26.0f));
    // The icons: the globe square at its size, the pad at its proportions centred in its square.
    const Eth::HudCmd& globe = cmds[26];
    CHECK(globe.pos == Eth::vector2(100.0f, 50.0f) && globe.size == Eth::vector2(40.0f, 40.0f));
    CHECK_EQ(globe.color, 0xC8FFFFFFu);
    const Eth::HudCmd& pad = cmds[27];
    CHECK_NEAR(pad.size.x, 60.0f);
    CHECK_NEAR(pad.size.y, 60.0f * 47.0f / 96.0f);
    CHECK_NEAR(pad.pos.y, 50.0f + (60.0f - 60.0f * 47.0f / 96.0f) * 0.5f);

    // Drawn: the big panel's corner takes the art's corner, its middle the art's middle.
    entt::registry registry;
    Render::TextureCache textures(kApp);
    Render::FontAtlas fonts;
    Render::Localization loc;
    loc.Load();
    Render::HudRenderer hud;
    hud.Attach(registry, textures, fonts, loc);
    Render::View view;
    view.logicalScreen = glm::vec2(1024.0f, 768.0f);
    view.windowPixels = glm::uvec2(1024, 768);
    view.scale = 1.0f;
    view.viewportMin = glm::vec2(0.0f);
    view.viewportMax = glm::vec2(1024.0f, 768.0f);
    std::vector<Supersonic::ScreenOverlay::Quad> quads;
    hud.Build(snapshot, view, quads);
    CHECK_EQ(quads.size(), cmds.size());
    if (quads.size() == cmds.size()) {
        CHECK_NEAR(quads[0].uvMin.x, 0.0f);
        CHECK_NEAR(quads[0].uvMax.x, 26.0f / 92.0f);
        CHECK_NEAR(quads[0].min.x, 10.0f / 1024.0f);
        CHECK_NEAR(quads[0].max.x, 36.0f / 1024.0f);
        CHECK_NEAR(quads[4].uvMin.x, 26.0f / 92.0f);
        CHECK_NEAR(quads[4].uvMax.x, 66.0f / 92.0f);
        CHECK_NEAR(quads[4].min.x, 36.0f / 1024.0f);
        CHECK_NEAR(quads[4].max.x, 284.0f / 1024.0f);
        CHECK_NEAR(quads[4].max.y, 134.0f / 768.0f);
    }
    hud.Detach();

    // Without a data folder: nothing at all, and nothing broken.
    Script::g_artDir.clear();
    machine.Frame(Eth::InputFrame{});
    CHECK_EQ(machine.Snapshot().hud.size(), std::size_t{0});
    Script::g_artDir = savedDir;
}

// ENHANCEMENT E28: THE LAYER, DRIVEN. Nothing before this attached PenumbraLayer to a bare registry: this runs the real   // E28
// game's options screen on a phone's layout through the layer's own frame loop (OnFixedUpdate then OnUpdate, one tick a   // E28
// frame), with --finger's synthetic fingers and the engine's key snapshot as the only inputs. It pins that the options   // E28
// screen's button opens the touch controls' editor; that the Machine stands still under it; that a finger drags the jump   // E28
// button and the lift is kept in the settings; that Esc closes the editor without the options screen reading it   // E28
// (PauseMenu::HoldPressed), and that a finger that was down at that moment clicks nothing. Nothing is written: no user   // E28
// directory, and noSave besides. Everything the layer reads of the machine is through its const accessors.   // E28
namespace {                                                                                                             // E28

bool MachineDrawsText(const Eth::Machine& machine, const std::string& needle) {                                         // E28
    for (const Eth::HudCmd& c : machine.Snapshot().hud) {                                                               // E28
        if (c.kind == Eth::HudCmd::Kind::Text && c.text.find(needle) != std::string::npos) return true;               // E28
    }                                                                                                                   // E28
    return false;                                                                                                       // E28
}                                                                                                                       // E28

// The editor's backdrop is the overlay's first command: a Rectangle over the whole shown area (here the 1024x768 screen).   // E28
bool OverlayHasBackdrop(const std::vector<Eth::HudCmd>& overlay) {                                                      // E28
    return !overlay.empty() && overlay.front().kind == Eth::HudCmd::Kind::Rectangle &&                                  // E28
           overlay.front().pos == Eth::vector2(0.0f, 0.0f) && overlay.front().size == Eth::vector2(1024.0f, 768.0f);    // E28
}                                                                                                                       // E28

void TestTouchEditorInLayer() {                                                                                         // E28
    const std::string savedArtDir = Script::g_artDir;                                                                   // E28
    const bool savedMobile = Script::g_mobileLayout;                                                                    // E28

    // Where things are, from the same code the layer lays out with: the options scene is laid out as the menus are      // E28
    // (1024x768, unit 1, no insets here), so the editor's tiles and the shipped manifest's controls are found from here.   // E28
    const Render::TouchGeometry geometry;                                                                               // E28
    const Render::TouchEditor::Layout tiles = Render::TouchEditor::ComputeLayout(geometry);                             // E28
    const glm::vec2 lockAt = tiles.widget[static_cast<std::size_t>(Render::TouchEditWidget::Lock)].Centre();            // E28
    const Render::TouchManifest manifest =                                                                              // E28
        Render::TouchControls::LoadManifest(std::filesystem::path(PENUMBRA_DATA_DIR) / Render::TouchControls::kManifestFile);   // E28
    const Render::TouchLayout controls =                                                                                // E28
        Render::TouchControls::ComputeLayout(manifest, glm::vec2(0.0f), glm::vec2(1024.0f, 768.0f), Render::TouchInsets{}, 1.0f);   // E28
    const glm::vec2 jumpAt = controls[Render::TouchControl::Jump].Centre();                                             // E28
    const glm::vec2 jumpTo = jumpAt + glm::vec2(-40.0f, 12.0f);                                                         // E28
    const glm::vec2 restingAt(300.0f, 207.0f);   // the touch switch's second row: where a click would turn the controls off   // E28

    PenumbraLayer::Options options;                                                                                     // E28
    options.userDir.clear();   // SaveSettings has nowhere to write; noSave says so as well                               // E28
    options.noSave = true;                                                                                              // E28
    options.startScene = "videoModes.esc";                                                                              // E28
    options.windowPixels = glm::uvec2(1024u, 768u);                                                                     // E28
    options.settings = Render::Settings::Defaults("en");                                                                // E28
    options.languageOverride = "en";                                                                                    // E28
    options.touchOverride = true;                                                                                       // E28
    options.mobileLayoutOverride = true;                                                                                // E28
    options.widescreenOverride = false;                                                                                 // E28
    options.smoothMotionOverride = false;                                                                               // E28
    options.pauseOnFocusLossOverride = false;                                                                           // E28
    options.safeAreaOverride = Supersonic::SafeAreaInsets{};                                                            // E28
    // Ticks counted from the first the layer runs, as --hold's are. The editor opens at tick 30 (the scene is up well          // E28
    // before: waited for below): the windows below all lie after that.                                                   // E28
    options.devFingers = {                                                                                              // E28
        PenumbraLayer::DevFinger{2, lockAt, lockAt, 100u, 101u},      // a tap on the padlock                           // E28
        PenumbraLayer::DevFinger{1, jumpAt, jumpTo, 110u, 116u},      // a drag of the jump button, 40 left and 12 down   // E28
        PenumbraLayer::DevFinger{3, restingAt, restingAt, 130u, 150u},   // down across the Esc that closes the editor   // E28
        PenumbraLayer::DevFinger{4, restingAt, restingAt, 170u, 172u},   // a tap after it: a click again               // E28
    };                                                                                                                  // E28

    entt::registry registry;                                                                                            // E28
    PenumbraLayer layer(options);                                                                                       // E28
    unsigned tick = 0;                                                                                                  // E28
    const auto step = [&] {   // the engine's frame: a fixed tick, then the draw                                        // E28
        layer.OnFixedUpdate(registry, PenumbraLayer::kTick);                                                            // E28
        layer.OnUpdate(registry, PenumbraLayer::kTick);                                                                 // E28
        ++tick;                                                                                                         // E28
    };                                                                                                                  // E28
    const auto runTo = [&](const unsigned target) {                                                                     // E28
        while (tick < target) step();                                                                                   // E28
    };                                                                                                                  // E28
    const auto scene = [&layer] { return layer.Machine()->GetSceneFileName(); };                                        // E28
    bool attached = false;                                                                                              // E28
    try {                                                                                                               // E28
        layer.OnAttach(registry);                                                                                       // E28
        attached = true;                                                                                                // E28
        Eth::Machine* machine = layer.Machine();                                                                        // E28
        CHECK(machine != nullptr);                                                                                      // E28
        while (scene() != "scenes/videoModes.esc" && tick < 80) step();                                                 // E28
        CHECK_MSG(scene() == "scenes/videoModes.esc", "the options scene never came up: " + scene());                  // E28
        runTo(30);                                                                                                      // E28
        CHECK(MachineDrawsText(*machine, "Ajustar controles"));   // the entry button, under the touch switch          // E28
        CHECK(!layer.EditorOpen());                                                                                     // E28
        CHECK(!OverlayHasBackdrop(layer.Overlay()));                                                                    // E28

        // The entry: the button raises the flag; the layer reads it at the end of that tick, lowers it, opens the editor.   // E28
        Script::g_adjustTouchControls = true;                                                                           // E28
        step();                                                                                                         // E28
        CHECK(layer.EditorOpen());                                                                                      // E28
        CHECK(layer.Editor().Locked());                                                                                 // E28
        CHECK(!Script::g_adjustTouchControls);                                                                          // E28
        // OnUpdate drew it: the backdrop first, over the whole screen, then the controls and the tiles.                  // E28
        CHECK(OverlayHasBackdrop(layer.Overlay()));                                                                     // E28
        CHECK(layer.Overlay().size() > 8u);                                                                             // E28
        CHECK(layer.Touch().Visible(Render::TouchControl::Jump));   // the real controls, laid out for the editor          // E28

        // The Machine stands still while it is open, as under the pause.                                              // E28
        const unsigned frozenAt = machine->FrameIndex();                                                                // E28
        runTo(tick + 6);                                                                                                // E28
        CHECK_EQ(machine->FrameIndex(), frozenAt);                                                                      // E28
        CHECK(layer.EditorOpen());                                                                                      // E28

        // The padlock, tapped by a synthetic finger (ticks 100-101), unlocks: the controls can be dragged now.          // E28
        runTo(106);                                                                                                     // E28
        CHECK(!layer.Editor().Locked());                                                                                // E28
        CHECK(!layer.Editor().Tuning().Moved());                                                                        // E28

        // The drag (ticks 110-116) and its lift: the jump button is 40 px left and 12 down of where it was, in the        // E28
        // controls the editor draws, and the tuning is kept in the settings (not written: noSave).                       // E28
        const glm::vec2 before = layer.Touch().Layout()[Render::TouchControl::Jump].min;                                // E28
        runTo(120);                                                                                                     // E28
        const glm::vec2 after = layer.Touch().Layout()[Render::TouchControl::Jump].min;                                 // E28
        std::printf("  jump: %.1f,%.1f -> %.1f,%.1f after the drag (wanted %.1f,%.1f)\n", before.x, before.y, after.x,   // E28
                    after.y, before.x - 40.0f, before.y + 12.0f);                                                       // E28
        CHECK_MSG(std::fabs(after.x - (before.x - 40.0f)) < 0.5f && std::fabs(after.y - (before.y + 12.0f)) < 0.5f,    // E28
                  "the jump button did not follow the finger");                                                         // E28
        CHECK(layer.Editor().Tuning().Moved());                                                                         // E28
        CHECK(layer.CurrentSettings().touchTuning.Moved());                                                             // E28
        CHECK(layer.CurrentSettings().touchTuning == layer.Editor().Tuning());                                          // E28
        CHECK_EQ(machine->FrameIndex(), frozenAt);                                                                      // E28

        // Esc closes it - with a finger down on the options screen's own switch (tick 130 on). The Esc must not reach    // E28
        // the options screen (waitForInputToMenu would leave it for the menu), nor must the finger click.               // E28
        runTo(133);                                                                                                     // E28
        CHECK(layer.EditorOpen());                                                                                      // E28
        CHECK_EQ(Script::g_touchControls.getCurrent(), 0u);                                                             // E28
        Supersonic::RawInputState escape;                                                                               // E28
        escape.keys[Supersonic::Key::Escape] = true;                                                                    // E28
        Supersonic::Input::Update(escape);                                                                              // E28
        step();                                                                                                         // E28
        CHECK(!layer.EditorOpen());                                                                                     // E28
        CHECK(!OverlayHasBackdrop(layer.Overlay()));                                                                    // E28
        CHECK(machine->FrameIndex() > frozenAt);   // the options screen runs again                                     // E28
        CHECK(!layer.Touch().Visible(Render::TouchControl::Jump));   // and the controls hide at once, not a frame later   // E28
        for (int held = 0; held < 6; ++held) {   // Esc still down: masked until it is up                              // E28
            step();                                                                                                     // E28
            CHECK_MSG(scene() == "scenes/videoModes.esc", "the Esc that closed the editor left the options screen: " + scene());   // E28
        }                                                                                                               // E28
        Supersonic::Input::Update(Supersonic::RawInputState{});                                                         // E28
        runTo(160);   // the resting finger lifted at 150 and clicked nothing                                           // E28
        CHECK(scene() == "scenes/videoModes.esc");                                                                      // E28
        CHECK_EQ(Script::g_touchControls.getCurrent(), 0u);                                                             // E28

        // And the screen answers again: a fresh finger (ticks 170-172) on the same row turns the touch controls off.     // E28
        runTo(180);                                                                                                     // E28
        std::printf("  a tap on the touch switch after the editor: row %u\n", Script::g_touchControls.getCurrent());     // E28
        CHECK_EQ(Script::g_touchControls.getCurrent(), 1u);                                                             // E28
        Script::g_touchControls.setCurrent(0u);                                                                         // E28

        // Esc is the options screen's own again: it goes back to the menu.                                              // E28
        Supersonic::Input::Update(escape);                                                                              // E28
        runTo(tick + 6);                                                                                                // E28
        Supersonic::Input::Update(Supersonic::RawInputState{});                                                         // E28
        CHECK_MSG(scene() == "scenes/menu.esc", "Esc no longer leaves the options screen: " + scene());                 // E28
    } catch (const std::exception& e) {                                                                                 // E28
        CHECK_MSG(false, std::string(attached ? "the layer threw at tick " + std::to_string(tick) + ": " : "OnAttach threw: ") + e.what());   // E28
    }                                                                                                                   // E28
    Supersonic::Input::Update(Supersonic::RawInputState{});                                                             // E28
    if (attached) layer.OnDetach(registry);                                                                             // E28
    Script::g_adjustTouchControls = false;                                                                              // E28
    Script::g_touchControls.setCurrent(0u);                                                                             // E28
    Script::g_mobileLayout = savedMobile;                                                                               // E28
    Script::g_artDir = savedArtDir;                                                                                     // E28
}                                                                                                                       // E28

} // namespace                                                                                                          // E28

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
    TestDisplayModeRowsFit();
    TestLanguages();          // E24
    TestLanguageFallback();
    TestLanguageFiles();
    TestRooms();
    TestFontCoverage();
    TestArabicShaping();
    TestAtlasBatching();
    TestHudRenderer();
    TestSpritePart();      // E26
    TestShapedSpritePart();   // E27
    TestOptionsArt();      // E27
    TestFitColumns();      // E25
    TestWideMenuHud();
    TestWideMenuBackdrop();
    TestWideMenuScene();   // it boots the real game, whose globals outlive it
    TestPlaqueInGame();    // E26: last, booting level 1 afresh (a new game)
    TestTouchEditorInLayer();   // E28: the layer itself, driven on a bare registry (its globals die with the process)
    return test::summary("test_pn_render_hud", 150);
}
