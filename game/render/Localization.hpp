#pragma once

// The languages the enhanced port can show in place of the original's
// Portuguese, looked up at the draw boundary (CLAUDE.md rule 8): the scripts
// keep composing their cp1252 strings exactly as the original did, and the HUD
// renderer asks for the translation of whatever a DrawText command carries.
//
// game/data/strings.json (UTF-8) holds the English, and what every language
// shares:
//
//   "languages": [ { "id": "en", "name": "English" }, ..., { "id": "ar", "name": ..., "rtl": true } ]
//   "strings":  { "<Portuguese>": "<English>" }   whole strings, exact
//   "patterns": [ { "pt": "Jogador {int} é o vencedor!", "en": "Player {1} wins!" } ]
//   "images":   { "<original image>": { "en": "<variant under game/data>", "labels": {...} } }
//   "touch":    { "<Portuguese>": { "pt": "<Portuguese>", "en": "<English>" } }
//
// ENHANCEMENT E24: each other language (render/Languages.hpp) has its own file
// beside it, game/data/strings/<id>.json, keyed by the same Portuguese:
//
//   "strings":  { "<Portuguese>": "<translation>" }
//   "patterns": [ { "pt": "<a strings.json pattern's pt>", "text": "<translation with {1}..{9}>" } ]
//   "touch":    { "<Portuguese>": "<translation of the touch wording>" }
//
// A pattern whose English is its Portuguese ("hp: {int}", "[{any}] {text}",
// "{int}x{int}") is the same in every language and comes from strings.json.
// Whatever a language's file lacks, or leaves empty, is drawn in ENGLISH
// (logged once per text and language); what English lacks stays Portuguese.
// Portuguese is the original and draws the scripts' text as it is.
//
// TEXT IS UTF-8 FROM HERE ON (E24). Translate takes the scripts' cp1252 and
// hands back UTF-8 - Portuguese through Eth::Cp1252ToUtf8, which keeps E21's
// 0x8D as U+0107 - for FontAtlas to lay out by code point, since Turkish,
// Cyrillic, Japanese and Arabic are not cp1252. English comes out as the very
// characters it always drew.
//
// LANGUAGE NAMES. The options screen's chooser names each language in its own
// script, the same in every language (Portuguese included), from "languages".
// Those names cannot be cp1252 literals in a script, so the chooser draws a
// key (LanguageNameKey) that Translate turns into the name in every language.
//
// TOUCH (ENHANCEMENT E16). The original's control hints name keys ("tecla
// 'S'", "CTRL", "as setas"): a help sign, the how-to-play panel, a lore sign.
// While the touch controls are on (SetTouch, which the layer sets from its
// switch every frame) a text "touch" lists is drawn in its touch wording
// instead, in the current language, naming the on-screen buttons. Whole
// strings only, normalised and trimmed as "strings" keys are; a text it does
// not list goes on as below. With touch off nothing changes: not a byte.
//
// PATTERNS are for the strings the scripts compose at run time ("hp: " + hp,
// "Jogador " + id + " \xE9 o vencedor!", a mana cost, a time). A pattern is the
// Portuguese with placeholders, matched against the WHOLE string:
//   {int}   an integer: an optional '-' and one or more digits
//   {any}   any run of characters, copied verbatim (a time list, a bullet)
//   {text}  any non-empty run, itself TRANSLATED (the arena blurb before a
//           composed suffix, a switch's label after its "[x] ")
// and the translation refers to them in order of appearance as {1}..{9}, so a
// translation may reorder them. Placeholders rather than regular expressions,
// because the Portuguese is full of what a regex would need escaped ('.', '?',
// '+', '[', '(') and a translator should be able to read the file.
//
// LOOKUP. The text is first normalised - line endings made LF, the spaces at
// the end of each line dropped (the heredocs of menu.as carry CRLF and stray
// trailing spaces, neither of which DrawText shows) - and its leading and
// trailing whitespace set aside ("Carregando...\n", "\n\nJogador 1 \xE9 o
// vencedor!\n"), to be put back around the translation, since the original
// laid those blank lines out on purpose. Then, on that core, in order:
//   1. "strings", exactly (keys are stored normalised and trimmed the same way);
//   2. "patterns", in strings.json's order;
//   3. the core split into paragraphs at blank lines, each translated alone -
//      how "arenaN" + a composed "you must finish the game first" suffix, and
//      the Versus screen's two concatenated messages, come out whole.
// A text with no letters at all (a time, a damage number, "[ ] ") is its own
// translation. Anything else not found is drawn in Portuguese and logged once.

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "render/Languages.hpp"

namespace Penumbra::Render {

class Localization {
public:
    Localization();

    // PENUMBRA_DATA_DIR "/strings.json".
    static std::string DefaultPath();

    // Reads and parses the file, then each language's strings/<id>.json in
    // the folder beside it (a missing one is logged: that language is drawn
    // in English). False (logged) when strings.json is missing or malformed;
    // Translate then hands every text back as it is.
    bool Load(const std::string& path = DefaultPath());
    // The same from a UTF-8 JSON document already in memory; `error` says why
    // not. Replaces whatever was loaded, every language's file included.
    bool LoadFromJson(const std::string& utf8Json, std::string& error);
    // E24: one language's file (strings/<id>.json) from memory, over a loaded
    // strings.json; replaces what that language had. Not English or Portuguese.
    bool LoadLanguageFromJson(Language language, const std::string& utf8Json, std::string& error);
    bool Loaded() const { return m_loaded; }
    // Whether a language has its own file loaded (English and Portuguese: once
    // strings.json is).
    bool HasLanguageFile(Language language) const;

    // The language Translate(text) and the HUD use. English by default: the
    // enhanced port's own language (a project decision); Portuguese is the original.
    void SetLanguage(Language language) { m_language = language; }
    Language CurrentLanguage() const { return m_language; }
    bool RightToLeft() const { return IsRightToLeft(m_language); }

    // E16: the touch controls are on, so a control hint "touch" lists is drawn
    // in its touch wording (TOUCH above). Kept across Load.
    void SetTouch(bool touch) { m_touch = touch; }
    bool Touch() const { return m_touch; }

    // cp1252 in, UTF-8 out. Portuguese returns the text as it is - but for a
    // control hint's touch wording while SetTouch is on, and a language name.
    std::string Translate(const std::string& cp1252, Language language) const;
    std::string Translate(const std::string& cp1252) const { return Translate(cp1252, m_language); }

    // Whether this cp1252 text is fully known in `language` - nothing drawn in
    // English in its place, nor in Portuguese (a text with no letters counts
    // as known; so does anything in Portuguese). For the suites and a log.
    bool HasTranslation(const std::string& cp1252, Language language = Language::English) const;
    // Whether "touch" has this text, worded in Portuguese and in English.
    bool HasTouchVariant(const std::string& cp1252) const;
    // E24: whether it has this text's touch wording in `language` itself.
    bool HasTouchVariant(const std::string& cp1252, Language language) const;
    std::size_t TouchCount() const { return m_touchStrings.size(); }

    // An image with the words baked in (the menu's buttons, the logo, the
    // "Voltar" arrow) as drawn in `language`, as an ABSOLUTE path; "" means
    // draw the original. Portuguese: always "". Another language: its own
    // variant, images/<id>/<the original's path> under the data folder, when
    // that file exists (E24); else the English one, the file strings.json's
    // "images" names when it exists. `relativePath` is the path the original
    // loaded ("entities/menu_buttons.png").
    std::string ImageVariant(const std::string& relativePath, Language language) const;

    // E24: what the options screen's chooser draws for a language, a cp1252
    // key ("{language:de}") that Translate turns into LanguageName in every
    // language; and that name, UTF-8 ("" when strings.json does not list it).
    static std::string LanguageNameKey(Language language);
    std::string LanguageName(Language language) const;

    std::size_t StringCount() const { return m_tables[LanguageIndex(Language::English)].strings.size(); }
    std::size_t StringCount(Language language) const { return m_tables[LanguageIndex(language)].strings.size(); }
    std::size_t PatternCount() const { return m_patterns.size(); }
    // E24: the Portuguese of the patterns every language takes from
    // strings.json as they are (their English is their Portuguese), in order.
    std::vector<std::string> SharedPatterns() const;

    // LF line endings, no spaces or tabs before a line break or at the end:
    // the form keys are stored and looked up in.
    static std::string Normalise(const std::string& cp1252);

private:
    enum class Slot { Literal, Int, Any, Text };
    struct Token {
        Slot slot = Slot::Literal;
        std::string literal;    // Literal only
    };
    struct OutToken {
        int capture = -1;       // 0-based capture, or -1 for literal
        std::string literal;    // UTF-8
    };
    using Output = std::vector<OutToken>;
    struct Pattern {
        std::vector<Token> pt;
        std::vector<Slot> captureSlots;  // one per placeholder, in order
        std::string source;              // the "pt" text (cp1252), for logs and a language file's lookup
        bool shared = false;             // its English is its Portuguese: every language draws it so
    };
    // One language's text: strings.json's English, or a strings/<id>.json.
    struct Table {
        bool loaded = false;
        std::unordered_map<std::string, std::string> strings;   // normalised, trimmed cp1252 -> UTF-8
        std::vector<std::optional<Output>> patterns;            // by m_patterns' index; none = not given
    };
    // A control hint's touch wording (UTF-8), one per language ("" = none).
    using TouchText = std::array<std::string, kLanguageCount>;
    // touchVariant's answer for a text as drawn: the wording it is listed
    // with (null: not listed) and the whitespace around it, to be put back.
    struct TouchHit {
        const TouchText* wording = nullptr;
        std::string before;
        std::string after;
    };
    // What translating a text came to: pieces nobody translated (drawn in
    // Portuguese), and pieces drawn in English for want of the language's own.
    struct Gaps {
        int missing = 0;
        int english = 0;
        Gaps& operator+=(const Gaps& other) {
            missing += other.missing;
            english += other.english;
            return *this;
        }
    };

    const TouchHit& touchVariant(const std::string& cp1252) const;
    // The touch wording of a text in `language` (the English one when the
    // language has none), its whitespace put back; "" when there is none.
    std::string touchWording(const TouchHit& hit, Language language) const;

    // Translates normalised cp1252 text into UTF-8; `out` always holds the
    // best effort.
    Gaps translateNormalised(const std::string& text, Language language, std::string& out, int depth) const;
    Gaps translateCore(const std::string& core, Language language, std::string& out, int depth) const;
    bool matchPattern(const Pattern& pattern, const std::string& core, std::vector<std::string>& captures) const;
    bool matchFrom(const Pattern& pattern, std::size_t token, const std::string& core, std::size_t pos,
                   std::vector<std::string>& captures, std::size_t capture) const;
    // The language name a LanguageNameKey stands for; null for any other text.
    const std::string* languageNameFor(const std::string& cp1252) const;

    static Pattern ParsePattern(const std::string& pt);
    static Output ParseOutput(const std::string& utf8);

    std::array<Table, kLanguageCount> m_tables;
    std::vector<Pattern> m_patterns;
    std::array<std::string, kLanguageCount> m_names;   // UTF-8
    std::map<std::string, std::string> m_images;   // lower-case original path -> English variant, relative to m_dataDir
    std::string m_dataDir;                          // PENUMBRA_DATA_DIR, or the loaded file's folder
    Language m_language = Language::English;
    bool m_loaded = false;
    // E16: normalised, trimmed Portuguese -> its touch wording.
    std::unordered_map<std::string, TouchText> m_touchStrings;
    bool m_touch = false;

    // Translate runs on every DrawText of every frame; most texts repeat, so
    // the answer is remembered, per language (bounded: a clock makes a new
    // string a second).
    mutable std::array<std::unordered_map<std::string, std::string>, kLanguageCount> m_memo;
    // touchVariant's answers by the text as drawn. Apart from m_memo, which
    // must not hand a touch wording back once touch is off.
    mutable std::unordered_map<std::string, TouchHit> m_touchMemo;
    mutable std::unordered_set<std::string> m_logged;
    // ImageVariant's answers by language and path, "" included, so a HUD
    // sprite costs no stat() - and a switch between two languages never hands
    // back the other one's file.
    mutable std::map<std::string, std::string> m_imageResolved;
};

} // namespace Penumbra::Render
