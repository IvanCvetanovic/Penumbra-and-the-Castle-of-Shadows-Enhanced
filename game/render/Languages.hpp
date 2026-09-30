#pragma once

// ENHANCEMENT E24: the languages the enhanced edition is drawn in. The
// original spoke Portuguese only; the English edition added English (E5), and
// E24 the nine others of the author's Magic Rampage Companion app, in the
// order its language picker lists them - which is the options screen's order
// too. The id is what settings.json, --lang, game/data/strings/<id>.json and
// game/data/images/<id>/ use.
//
// The names the chooser shows (each in its own script, the same in every
// language) are strings.json's "languages", not C++ literals: MSVC reads this
// source in the system's code page, and Cyrillic, Japanese and Arabic would
// not survive it (Localization::LanguageName).

#include <array>
#include <cstddef>
#include <string_view>

namespace Penumbra::Render {

enum class Language : unsigned char {
    English,
    German,
    Spanish,
    French,
    Italian,
    Portuguese,   // the original's
    Russian,
    Turkish,
    Ukrainian,
    Japanese,
    Arabic,
};

inline constexpr std::size_t kLanguageCount = 11;

struct LanguageInfo {
    Language language;
    const char* id;
    bool rightToLeft;
};

inline constexpr std::array<LanguageInfo, kLanguageCount> kLanguages = {{
    {Language::English, "en", false},
    {Language::German, "de", false},
    {Language::Spanish, "es", false},
    {Language::French, "fr", false},
    {Language::Italian, "it", false},
    {Language::Portuguese, "pt", false},
    {Language::Russian, "ru", false},
    {Language::Turkish, "tr", false},
    {Language::Ukrainian, "uk", false},
    {Language::Japanese, "ja", false},
    {Language::Arabic, "ar", true},
}};

constexpr std::size_t LanguageIndex(const Language language) { return static_cast<std::size_t>(language); }
constexpr const LanguageInfo& LanguageInfoOf(const Language language) { return kLanguages[LanguageIndex(language)]; }
constexpr const char* LanguageId(const Language language) { return LanguageInfoOf(language).id; }
constexpr bool IsRightToLeft(const Language language) { return LanguageInfoOf(language).rightToLeft; }

// The table's entry for an id, in any case ("PT" is Portuguese); false, and
// `language` untouched, for anything else.
constexpr bool LanguageFromId(const std::string_view id, Language& language) {
    const auto lower = [](const char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
    for (const LanguageInfo& info : kLanguages) {
        const std::string_view known(info.id);
        if (known.size() != id.size()) continue;
        bool same = true;
        for (std::size_t i = 0; i < id.size() && same; ++i) same = lower(id[i]) == known[i];
        if (same) {
            language = info.language;
            return true;
        }
    }
    return false;
}

static_assert(kLanguages[LanguageIndex(Language::Arabic)].language == Language::Arabic,
              "kLanguages is in the enum's order");
static_assert(kLanguages[LanguageIndex(Language::Portuguese)].language == Language::Portuguese,
              "kLanguages is in the enum's order");

} // namespace Penumbra::Render
