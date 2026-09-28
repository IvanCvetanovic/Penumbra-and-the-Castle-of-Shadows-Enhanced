#pragma once

// The English the enhanced port can show in place of the original's
// Portuguese, looked up at the draw boundary (CLAUDE.md rule 8): the scripts
// keep composing their cp1252 strings exactly as the original did, and the HUD
// renderer asks for the translation of whatever a DrawText command carries.
//
// game/data/strings.json (UTF-8) holds it:
//
//   "strings":  { "<Portuguese>": "<English>" }   whole strings, exact
//   "patterns": [ { "pt": "Jogador {int} é o vencedor!", "en": "Player {1} wins!" } ]
//   "images":   { "<original image>": { "en": "<variant under game/data>", "labels": {...} } }
//   "touch":    { "<Portuguese>": { "pt": "<Portuguese>", "en": "<English>" } }
//
// TOUCH (ENHANCEMENT E16). The original's control hints name keys ("tecla
// 'S'", "CTRL", "as setas"): a help sign, the how-to-play panel, a lore sign.
// While the touch controls are on (SetTouch, which the layer sets from its
// switch every frame) a text "touch" lists is drawn in its touch wording
// instead, in either language, naming the on-screen buttons. Whole strings
// only, normalised and trimmed as "strings" keys are; a text it does not list
// goes on as below. With touch off nothing changes: not a byte.
//
// PATTERNS are for the strings the scripts compose at run time ("hp: " + hp,
// "Jogador " + id + " \xE9 o vencedor!", a mana cost, a time). A pattern is the
// Portuguese with placeholders, matched against the WHOLE string:
//   {int}   an integer: an optional '-' and one or more digits
//   {any}   any run of characters, copied verbatim (a time list, a bullet)
//   {text}  any non-empty run, itself TRANSLATED (the arena blurb before a
//           composed suffix, a switch's label after its "[x] ")
// and the English refers to them in order of appearance as {1}..{9}, so a
// translation may reorder them. Placeholders rather than regular expressions,
// because the Portuguese is full of what a regex would need escaped ('.', '?',
// '+', '[', '(') and a translator should be able to read the file.
//
// LOOKUP. The text is first normalised - line endings made LF, the spaces at
// the end of each line dropped (the heredocs of menu.as carry CRLF and stray
// trailing spaces, neither of which DrawText shows) - and its leading and
// trailing whitespace set aside ("Carregando...\n", "\n\nJogador 1 \xE9 o
// vencedor!\n"), to be put back around the English, since the original laid
// those blank lines out on purpose. Then, on that core, in order:
//   1. "strings", exactly (keys are stored normalised and trimmed the same way);
//   2. "patterns", in file order;
//   3. the core split into paragraphs at blank lines, each translated alone -
//      how "arenaN" + a composed "you must finish the game first" suffix, and
//      the Versus screen's two concatenated messages, come out whole.
// A text with no letters at all (a time, a damage number, "[ ] ") is its own
// translation. Anything else not found is drawn in Portuguese and logged once.

#include <cstddef>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Penumbra::Render {

enum class Language { Portuguese, English };

class Localization {
public:
    Localization();

    // PENUMBRA_DATA_DIR "/strings.json".
    static std::string DefaultPath();

    // Reads and parses the file. False (logged) when it is missing or
    // malformed; Translate then hands every text back unchanged.
    bool Load(const std::string& path = DefaultPath());
    // The same from a UTF-8 JSON document already in memory; `error` says why
    // not. Replaces whatever was loaded.
    bool LoadFromJson(const std::string& utf8Json, std::string& error);
    bool Loaded() const { return m_loaded; }

    // The language Translate(text) and the HUD use. English by default: the
    // enhanced port's own language (Ivan's ruling); Portuguese is the original.
    void SetLanguage(Language language) { m_language = language; }
    Language CurrentLanguage() const { return m_language; }

    // E16: the touch controls are on, so a control hint "touch" lists is drawn
    // in its touch wording (TOUCH above). Kept across Load.
    void SetTouch(bool touch) { m_touch = touch; }
    bool Touch() const { return m_touch; }

    // cp1252 in, cp1252 out. Portuguese returns the text untouched - but for a
    // control hint's touch wording, while SetTouch is on.
    std::string Translate(const std::string& cp1252, Language language) const;
    std::string Translate(const std::string& cp1252) const { return Translate(cp1252, m_language); }

    // Whether the English for this cp1252 text is fully known (a text with no
    // letters counts as known). For the suites and for a coverage log.
    bool HasTranslation(const std::string& cp1252) const;
    // Whether "touch" has this text, worded in both languages.
    bool HasTouchVariant(const std::string& cp1252) const;
    std::size_t TouchCount() const { return m_touchStrings.size(); }

    // An English variant of an image whose words are baked in (the menu's
    // buttons, the logo, the "Voltar" arrow), as an ABSOLUTE path, when
    // strings.json names one and it exists; "" means draw the original.
    // `relativePath` is the path the original loaded ("entities/menu_buttons.png").
    std::string ImageVariant(const std::string& relativePath, Language language) const;

    std::size_t StringCount() const { return m_strings.size(); }
    std::size_t PatternCount() const { return m_patterns.size(); }

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
        std::string literal;
    };
    struct Pattern {
        std::vector<Token> pt;
        std::vector<OutToken> en;
        std::vector<Slot> captureSlots;  // one per placeholder, in order
        std::string source;              // the "pt" text, for logs
    };
    // A control hint's touch wording, one per language ("" = none).
    struct TouchText {
        std::string pt;
        std::string en;
    };

    // The touch wording of a text as drawn, its own surrounding whitespace
    // put back; nullptr when "touch" does not list it.
    const TouchText* touchVariant(const std::string& cp1252) const;

    // Translates normalised text; returns how many pieces stayed untranslated
    // (0 = all of it). `out` always holds the best effort.
    int translateNormalised(const std::string& text, std::string& out, int depth) const;
    int translateCore(const std::string& core, std::string& out, int depth) const;
    bool matchPattern(const Pattern& pattern, const std::string& core, std::vector<std::string>& captures) const;
    bool matchFrom(const Pattern& pattern, std::size_t token, const std::string& core, std::size_t pos,
                   std::vector<std::string>& captures, std::size_t capture) const;

    static Pattern ParsePattern(const std::string& pt, const std::string& en);

    std::unordered_map<std::string, std::string> m_strings;
    std::vector<Pattern> m_patterns;
    std::map<std::string, std::string> m_images;   // lower-case original path -> variant, relative to m_dataDir
    std::string m_dataDir;                          // PENUMBRA_DATA_DIR, or the loaded file's folder
    Language m_language = Language::English;
    bool m_loaded = false;
    // E16: normalised, trimmed Portuguese -> its touch wording.
    std::unordered_map<std::string, TouchText> m_touchStrings;
    bool m_touch = false;

    // Translate runs on every DrawText of every frame; most texts repeat, so
    // the answer is remembered (bounded: a clock makes a new string a second).
    mutable std::unordered_map<std::string, std::string> m_memo;
    // touchVariant's answers by the text as drawn, both languages at once (an
    // empty pair: not listed). Apart from m_memo, which holds English only
    // and must not hand a touch wording back once touch is off.
    mutable std::unordered_map<std::string, TouchText> m_touchMemo;
    mutable std::unordered_set<std::string> m_logged;
    // ImageVariant's answers, "" included, so a HUD sprite costs no stat().
    mutable std::map<std::string, std::string> m_imageResolved;
};

} // namespace Penumbra::Render
