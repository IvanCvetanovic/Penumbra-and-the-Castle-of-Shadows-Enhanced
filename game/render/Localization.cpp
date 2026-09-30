#include "render/Localization.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>

#include "core/Json.hpp"
#include "core/Log.hpp"
#include "eth/Paths.hpp"
#include "eth/Text.hpp"

// A compile definition on PenumbraGame (game/CMakeLists.txt); the fallback only
// keeps a stray translation unit compiling, and finds nothing.
#ifndef PENUMBRA_DATA_DIR
#define PENUMBRA_DATA_DIR ""
#endif

namespace Penumbra::Render {
namespace {

// Recursion only descends into strictly shorter pieces, so this is a guard
// against a pathological file, not a limit any real text reaches.
constexpr int kMaxDepth = 6;
// A clock drawn with shadowText makes two new strings a second; the memo is
// cleared when it fills rather than grown forever.
constexpr std::size_t kMemoCap = 4096;
constexpr std::size_t kLogCap = 256;

bool IsSpace(const char c) { return c == ' ' || c == '\n' || c == '\t'; }

// Whether a text holds anything a translation could change. Digits, ':', '-',
// brackets and the 0x95 bullet are the same in every language, so the timer,
// the damage numbers and an image switch's "[ ] " never reach the tables.
bool HasLetters(const std::string& text) {
    for (const char c : text) {
        const auto b = static_cast<unsigned char>(c);
        if ((b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') || b >= 0xC0) return true;
    }
    return false;
}

std::string Trimmed(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && IsSpace(text[begin])) ++begin;
    while (end > begin && IsSpace(text[end - 1])) --end;
    return text.substr(begin, end - begin);
}

std::string PathKey(std::string path) {
    for (char& c : path) {
        if (c == '\\') c = '/';
        else if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return path;
}

bool ReadFile(const std::string& path, std::string& out) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) return false;
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

// A BOM is what an editor on Windows may leave; the parser would call it
// content before the document.
std::string WithoutBom(const std::string& utf8Json) {
    const std::string_view bom = "\xEF\xBB\xBF";
    return utf8Json.compare(0, bom.size(), bom) == 0 ? utf8Json.substr(bom.size()) : utf8Json;
}

// A key as the scripts' text is looked up: cp1252, normalised, trimmed.
std::string KeyOf(const std::string& utf8) { return Trimmed(Localization::Normalise(Eth::Utf8ToCp1252(utf8))); }

// A translation as it is drawn: UTF-8, with the key's normalisation. Normalise
// and Trimmed only touch ASCII bytes, which a UTF-8 sequence never contains.
std::string ValueOf(const std::string& utf8) { return Trimmed(Localization::Normalise(utf8)); }

} // namespace

Localization::Localization() : m_dataDir(PENUMBRA_DATA_DIR) {}

std::string Localization::DefaultPath() { return std::string(PENUMBRA_DATA_DIR) + "/strings.json"; }

std::string Localization::Normalise(const std::string& cp1252) {
    std::string out;
    out.reserve(cp1252.size());
    const auto dropLineEnd = [&out]() {
        while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) out.pop_back();
    };
    for (std::size_t i = 0; i < cp1252.size(); ++i) {
        char c = cp1252[i];
        if (c == '\r') {
            // CRLF is one break, as DrawText reads it; a lone CR breaks too.
            if (i + 1 < cp1252.size() && cp1252[i + 1] == '\n') continue;
            c = '\n';
        }
        if (c == '\n') dropLineEnd();
        out += c;
    }
    dropLineEnd();
    return out;
}

bool Localization::Load(const std::string& path) {
    std::string text;
    const std::filesystem::path named(path);
    if (!ReadFile(named.has_parent_path() ? Eth::ResolveUnder(named.parent_path().generic_string(),
                                                              named.filename().generic_string())
                                          : path,
                  text)) {
        SUPERSONIC_LOG_WARN("Penumbra") << "localization: cannot read " << path << "; text stays Portuguese";
        m_loaded = false;
        return false;
    }
    std::string error;
    if (!LoadFromJson(text, error)) {
        SUPERSONIC_LOG_WARN("Penumbra") << "localization: " << path << ": " << error << "; text stays Portuguese";
        return false;
    }
    // Image variants and the languages' files are named relative to it.
    m_dataDir = named.parent_path().generic_string();

    // E24: every other language's file. One missing is not an error: that
    // language is drawn in English, which the log says once here rather than
    // once per text.
    for (const LanguageInfo& info : kLanguages) {
        if (info.language == Language::English || info.language == Language::Portuguese) continue;
        const std::string file = std::string("strings/") + info.id + ".json";
        std::string json;
        if (!ReadFile(Eth::ResolveUnder(m_dataDir, file), json)) {
            SUPERSONIC_LOG_WARN("Penumbra") << "localization: no " << file << "; " << info.id << " is drawn in English";
            continue;
        }
        if (!LoadLanguageFromJson(info.language, json, error)) {
            SUPERSONIC_LOG_WARN("Penumbra") << "localization: " << file << ": " << error << "; " << info.id
                                            << " is drawn in English";
        }
    }
    return true;
}

bool Localization::LoadFromJson(const std::string& utf8Json, std::string& error) {
    for (Table& table : m_tables) table = Table{};
    m_patterns.clear();
    for (std::string& name : m_names) name.clear();
    m_images.clear();
    for (auto& memo : m_memo) memo.clear();
    m_imageResolved.clear();
    m_touchStrings.clear();
    m_touchMemo.clear();
    m_logged.clear();
    m_loaded = false;

    Supersonic::Json::Value root;
    if (!Supersonic::Json::Parse(WithoutBom(utf8Json), root, error)) return false;
    if (!root.IsObject()) {
        error = "the document is not an object";
        return false;
    }

    Table& english = m_tables[LanguageIndex(Language::English)];
    // Keys are trimmed as well as normalised, because lookup trims the text and
    // puts its own surrounding whitespace back around the translation.
    for (const auto& [key, value] : root["strings"].AsObject()) {
        if (key.empty() || key[0] == '_' || !value.IsString()) continue;   // "_..." are notes
        english.strings[KeyOf(key)] = ValueOf(value.AsString());
    }
    for (const Supersonic::Json::Value& entry : root["patterns"].AsArray()) {
        const std::string pt = entry["pt"].AsString();
        const std::string en = entry["en"].AsString();
        if (pt.empty()) continue;
        Pattern pattern = ParsePattern(KeyOf(pt));
        const Output output = ParseOutput(ValueOf(en));
        // The same in every language when the English is the Portuguese with
        // its placeholders numbered in order: numbers and markers only.
        std::string numbered;
        int next = 0;
        for (const Token& token : pattern.pt) {
            numbered += token.slot == Slot::Literal ? Eth::Cp1252ToUtf8(token.literal)
                                                    : "{" + std::to_string(++next) + "}";
        }
        pattern.shared = numbered == ValueOf(en);
        m_patterns.push_back(std::move(pattern));
        english.patterns.emplace_back(output);
    }
    for (const auto& [path, entry] : root["images"].AsObject()) {
        if (path.empty() || path[0] == '_') continue;
        m_images[PathKey(path)] = entry["en"].AsString();
    }
    // E16: keyed as "strings" is, so a hint is found however the original
    // spelled its line ends.
    for (const auto& [key, entry] : root["touch"].AsObject()) {
        if (key.empty() || key[0] == '_' || !entry.IsObject()) continue;
        TouchText& wording = m_touchStrings[KeyOf(key)];
        wording[LanguageIndex(Language::Portuguese)] = ValueOf(entry["pt"].AsString());
        wording[LanguageIndex(Language::English)] = ValueOf(entry["en"].AsString());
    }
    // E24: the languages' own names. The ids are Languages.hpp's (a suite
    // checks the list against it); one this build does not know is skipped.
    for (const Supersonic::Json::Value& entry : root["languages"].AsArray()) {
        Language language = Language::English;
        if (LanguageFromId(entry["id"].AsString(), language)) m_names[LanguageIndex(language)] = entry["name"].AsString();
    }
    english.loaded = true;
    m_tables[LanguageIndex(Language::Portuguese)].loaded = true;
    m_loaded = true;
    return true;
}

bool Localization::LoadLanguageFromJson(const Language language, const std::string& utf8Json, std::string& error) {
    if (language == Language::English || language == Language::Portuguese) {
        error = "English and Portuguese are strings.json's";
        return false;
    }
    Supersonic::Json::Value root;
    if (!Supersonic::Json::Parse(WithoutBom(utf8Json), root, error)) return false;
    if (!root.IsObject()) {
        error = "the document is not an object";
        return false;
    }

    Table table;
    // An empty value is a text not translated yet: left out, so it falls back.
    for (const auto& [key, value] : root["strings"].AsObject()) {
        if (key.empty() || key[0] == '_' || !value.IsString() || ValueOf(value.AsString()).empty()) continue;
        table.strings[KeyOf(key)] = ValueOf(value.AsString());
    }
    table.patterns.resize(m_patterns.size());
    for (const Supersonic::Json::Value& entry : root["patterns"].AsArray()) {
        const std::string pt = KeyOf(entry["pt"].AsString());
        const std::string text = ValueOf(entry["text"].AsString());
        if (pt.empty() || text.empty()) continue;
        bool found = false;
        for (std::size_t i = 0; i < m_patterns.size() && !found; ++i) {
            if (m_patterns[i].source != pt) continue;
            table.patterns[i] = ParseOutput(text);
            found = true;
        }
        if (!found) {
            SUPERSONIC_LOG_WARN("Penumbra") << "localization: " << LanguageId(language) << ": no pattern \""
                                            << Eth::Cp1252ToUtf8(pt) << "\" in strings.json";
        }
    }
    for (auto& entry : m_touchStrings) entry.second[LanguageIndex(language)].clear();
    for (const auto& [key, value] : root["touch"].AsObject()) {
        if (key.empty() || key[0] == '_' || !value.IsString()) continue;
        const auto it = m_touchStrings.find(KeyOf(key));
        if (it == m_touchStrings.end()) {
            SUPERSONIC_LOG_WARN("Penumbra") << "localization: " << LanguageId(language) << ": \"" << key
                                            << "\" is not in strings.json's touch";
            continue;
        }
        it->second[LanguageIndex(language)] = ValueOf(value.AsString());
    }
    table.loaded = true;
    m_tables[LanguageIndex(language)] = std::move(table);
    m_memo[LanguageIndex(language)].clear();
    return true;
}

bool Localization::HasLanguageFile(const Language language) const { return m_tables[LanguageIndex(language)].loaded; }

std::string Localization::LanguageNameKey(const Language language) {
    return std::string("{language:") + LanguageId(language) + "}";
}

std::string Localization::LanguageName(const Language language) const { return m_names[LanguageIndex(language)]; }

const std::string* Localization::languageNameFor(const std::string& cp1252) const {
    // "{language:" + an id of two letters + "}": the one shape to look at.
    constexpr std::string_view kOpen = "{language:";
    if (cp1252.size() != kOpen.size() + 3 || cp1252.compare(0, kOpen.size(), kOpen) != 0 || cp1252.back() != '}') {
        return nullptr;
    }
    Language language = Language::English;
    if (!LanguageFromId(std::string_view(cp1252).substr(kOpen.size(), 2), language)) return nullptr;
    const std::string& name = m_names[LanguageIndex(language)];
    return name.empty() ? nullptr : &name;
}

std::vector<std::string> Localization::SharedPatterns() const {
    std::vector<std::string> shared;
    for (const Pattern& pattern : m_patterns) {
        if (pattern.shared) shared.push_back(pattern.source);
    }
    return shared;
}

Localization::Pattern Localization::ParsePattern(const std::string& pt) {
    Pattern pattern;
    pattern.source = pt;
    std::string literal;
    const auto flush = [&]() {
        if (literal.empty()) return;
        Token token;
        token.literal = std::move(literal);
        pattern.pt.push_back(std::move(token));
        literal.clear();
    };
    for (std::size_t i = 0; i < pt.size(); ++i) {
        if (pt[i] == '{') {
            const std::size_t close = pt.find('}', i);
            if (close != std::string::npos) {
                const std::string name = pt.substr(i + 1, close - i - 1);
                Slot slot = Slot::Literal;
                if (name == "int") slot = Slot::Int;
                else if (name == "any") slot = Slot::Any;
                else if (name == "text") slot = Slot::Text;
                if (slot != Slot::Literal) {
                    flush();
                    Token token;
                    token.slot = slot;
                    pattern.pt.push_back(token);
                    pattern.captureSlots.push_back(slot);
                    i = close;
                    continue;
                }
            }
        }
        literal += pt[i];
    }
    flush();
    return pattern;
}

Localization::Output Localization::ParseOutput(const std::string& utf8) {
    Output output;
    std::string text;
    const auto flushOut = [&]() {
        if (text.empty()) return;
        OutToken token;
        token.literal = std::move(text);
        output.push_back(std::move(token));
        text.clear();
    };
    for (std::size_t i = 0; i < utf8.size(); ++i) {
        if (utf8[i] == '{' && i + 2 < utf8.size() && utf8[i + 1] >= '1' && utf8[i + 1] <= '9' && utf8[i + 2] == '}') {
            flushOut();
            OutToken token;
            token.capture = utf8[i + 1] - '1';
            output.push_back(token);
            i += 2;
            continue;
        }
        text += utf8[i];
    }
    flushOut();
    return output;
}

bool Localization::matchFrom(const Pattern& pattern, const std::size_t token, const std::string& core,
                             const std::size_t pos, std::vector<std::string>& captures,
                             const std::size_t capture) const {
    if (token == pattern.pt.size()) return pos == core.size();
    const Token& current = pattern.pt[token];
    const std::size_t size = core.size();

    if (current.slot == Slot::Literal) {
        if (core.compare(pos, current.literal.size(), current.literal) != 0) return false;
        return matchFrom(pattern, token + 1, core, pos + current.literal.size(), captures, capture);
    }

    if (current.slot == Slot::Int) {
        std::size_t digits = pos;
        if (digits < size && core[digits] == '-') ++digits;
        std::size_t end = digits;
        while (end < size && core[end] >= '0' && core[end] <= '9') ++end;
        for (; end > digits; --end) {
            captures[capture] = core.substr(pos, end - pos);
            if (matchFrom(pattern, token + 1, core, end, captures, capture + 1)) return true;
        }
        return false;
    }

    // {any} / {text}: longest first, and only where the literal that follows
    // (if one does) actually begins - the only places a match can continue.
    const std::size_t minimum = current.slot == Slot::Text ? 1 : 0;
    if (pos + minimum > size) return false;
    const Token* next = token + 1 < pattern.pt.size() ? &pattern.pt[token + 1] : nullptr;
    for (std::size_t end = size;; --end) {
        const bool continues = next == nullptr || next->slot != Slot::Literal ||
                               core.compare(end, next->literal.size(), next->literal) == 0;
        if (continues) {
            captures[capture] = core.substr(pos, end - pos);
            if (matchFrom(pattern, token + 1, core, end, captures, capture + 1)) return true;
        }
        if (end == pos + minimum) break;
    }
    return false;
}

bool Localization::matchPattern(const Pattern& pattern, const std::string& core,
                                std::vector<std::string>& captures) const {
    captures.assign(pattern.captureSlots.size(), std::string());
    return matchFrom(pattern, 0, core, 0, captures, 0);
}

Localization::Gaps Localization::translateNormalised(const std::string& text, const Language language,
                                                     std::string& out, const int depth) const {
    if (depth > kMaxDepth) {
        out = Eth::Cp1252ToUtf8(text);
        return Gaps{1, 0};
    }
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && IsSpace(text[begin])) ++begin;
    while (end > begin && IsSpace(text[end - 1])) --end;
    if (begin == end) {
        out = text;   // whitespace only: ASCII
        return Gaps{};
    }
    std::string core;
    const Gaps gaps = translateCore(text.substr(begin, end - begin), language, core, depth);
    out = text.substr(0, begin) + core + text.substr(end);
    return gaps;
}

Localization::Gaps Localization::translateCore(const std::string& core, const Language language, std::string& out,
                                               const int depth) const {
    if (!HasLetters(core)) {
        out = Eth::Cp1252ToUtf8(core);
        return Gaps{};
    }
    const Table& own = m_tables[LanguageIndex(language)];
    const Table& english = m_tables[LanguageIndex(Language::English)];
    if (const auto it = own.strings.find(core); it != own.strings.end()) {
        out = it->second;
        return Gaps{};
    }
    if (&own != &english) {
        if (const auto it = english.strings.find(core); it != english.strings.end()) {
            out = it->second;
            return Gaps{0, 1};
        }
    }

    std::vector<std::string> captures;
    for (std::size_t p = 0; p < m_patterns.size(); ++p) {
        const Pattern& pattern = m_patterns[p];
        if (!matchPattern(pattern, core, captures)) continue;
        // The language's own wording; for a shared pattern, or one it lacks,
        // the English (only the latter a gap).
        Gaps gaps;
        const std::optional<Output>* output = p < own.patterns.size() ? &own.patterns[p] : nullptr;
        if (output == nullptr || !output->has_value()) {
            output = p < english.patterns.size() ? &english.patterns[p] : nullptr;
            if (!pattern.shared && &own != &english) gaps.english = 1;
        }
        if (output == nullptr || !output->has_value()) continue;
        out.clear();
        for (const OutToken& token : **output) {
            if (token.capture < 0) {
                out += token.literal;
                continue;
            }
            const auto index = static_cast<std::size_t>(token.capture);
            if (index >= captures.size()) {
                // "{4}" in a pattern with three placeholders: a typo in the
                // file, shown rather than hidden.
                out += '{';
                out += static_cast<char>('1' + token.capture);
                out += '}';
                continue;
            }
            if (pattern.captureSlots[index] == Slot::Text) {
                std::string piece;
                gaps += translateNormalised(captures[index], language, piece, depth + 1);
                out += piece;
            } else {
                out += Eth::Cp1252ToUtf8(captures[index]);
            }
        }
        return gaps;
    }

    // Paragraphs: split at every run of two or more line breaks, the runs kept.
    std::vector<std::string> pieces;
    std::vector<std::string> breaks;
    std::size_t start = 0;
    for (std::size_t i = 0; i + 1 < core.size();) {
        if (core[i] == '\n' && core[i + 1] == '\n') {
            std::size_t run = i;
            while (run < core.size() && core[run] == '\n') ++run;
            pieces.push_back(core.substr(start, i - start));
            breaks.push_back(core.substr(i, run - i));
            start = run;
            i = run;
        } else {
            ++i;
        }
    }
    if (!pieces.empty()) {
        pieces.push_back(core.substr(start));
        Gaps gaps;
        out.clear();
        for (std::size_t i = 0; i < pieces.size(); ++i) {
            std::string piece;
            gaps += translateNormalised(pieces[i], language, piece, depth + 1);
            out += piece;
            if (i < breaks.size()) out += breaks[i];
        }
        return gaps;
    }

    out = Eth::Cp1252ToUtf8(core);
    return Gaps{1, 0};
}

const Localization::TouchHit& Localization::touchVariant(const std::string& cp1252) const {
    auto it = m_touchMemo.find(cp1252);
    if (it == m_touchMemo.end()) {
        TouchHit hit;
        if (!m_touchStrings.empty() && HasLetters(cp1252)) {
            const std::string text = Normalise(cp1252);
            std::size_t begin = 0;
            std::size_t end = text.size();
            while (begin < end && IsSpace(text[begin])) ++begin;
            while (end > begin && IsSpace(text[end - 1])) --end;
            if (const auto found = m_touchStrings.find(text.substr(begin, end - begin));
                found != m_touchStrings.end()) {
                // The blank lines around it, as translateNormalised keeps them.
                hit.wording = &found->second;
                hit.before = text.substr(0, begin);
                hit.after = text.substr(end);
            }
        }
        if (m_touchMemo.size() >= kMemoCap) m_touchMemo.clear();
        it = m_touchMemo.emplace(cp1252, std::move(hit)).first;
    }
    return it->second;
}

std::string Localization::touchWording(const TouchHit& hit, const Language language) const {
    if (hit.wording == nullptr) return {};
    const std::string* core = &(*hit.wording)[LanguageIndex(language)];
    if (core->empty() && language != Language::Portuguese) core = &(*hit.wording)[LanguageIndex(Language::English)];
    return core->empty() ? std::string() : hit.before + *core + hit.after;
}

bool Localization::HasTouchVariant(const std::string& cp1252) const {
    const TouchHit& hit = touchVariant(cp1252);
    return hit.wording != nullptr && !(*hit.wording)[LanguageIndex(Language::Portuguese)].empty() &&
           !(*hit.wording)[LanguageIndex(Language::English)].empty();
}

bool Localization::HasTouchVariant(const std::string& cp1252, const Language language) const {
    const TouchHit& hit = touchVariant(cp1252);
    return hit.wording != nullptr && !(*hit.wording)[LanguageIndex(language)].empty();
}

std::string Localization::Translate(const std::string& cp1252, const Language language) const {
    // E24: a language's name, the same in every language.
    if (const std::string* name = languageNameFor(cp1252); name != nullptr) return *name;
    // E16: a control hint's touch wording. Only while touch is on, so with it
    // off every text takes the path it always took.
    if (m_touch) {
        const TouchHit& hit = touchVariant(cp1252);
        if (std::string worded = touchWording(hit, language); !worded.empty()) {
            const bool english = (*hit.wording)[LanguageIndex(language)].empty();
            const std::string logKey = std::string("touch:") + LanguageId(language) + ":" + cp1252;
            if (english && HasLanguageFile(language) && m_logged.size() < kLogCap && m_logged.insert(logKey).second) {
                SUPERSONIC_LOG_WARN("Penumbra") << "localization: no " << LanguageId(language)
                                                << " touch wording for \"" << Eth::Cp1252ToUtf8(cp1252)
                                                << "\"; drawn in English";
            }
            return worded;
        }
    }
    if (language == Language::Portuguese || !m_loaded || !HasLetters(cp1252)) return Eth::Cp1252ToUtf8(cp1252);
    auto& memo = m_memo[LanguageIndex(language)];
    if (const auto it = memo.find(cp1252); it != memo.end()) return it->second;

    std::string out;
    const Gaps gaps = translateNormalised(Normalise(cp1252), language, out, 0);
    const std::string logKey = std::string(LanguageId(language)) + ":" + cp1252;
    // A language without its file was said once at Load; English falls back to nothing.
    const bool logEnglish = gaps.english > 0 && HasLanguageFile(language);
    if ((gaps.missing > 0 || logEnglish) && m_logged.size() < kLogCap && m_logged.insert(logKey).second) {
        if (gaps.missing > 0) {
            SUPERSONIC_LOG_WARN("Penumbra") << "localization: no " << LanguageId(language) << " for \""
                                            << Eth::Cp1252ToUtf8(cp1252) << "\" (" << gaps.missing
                                            << " piece(s) left in Portuguese)";
        } else {
            SUPERSONIC_LOG_WARN("Penumbra") << "localization: no " << LanguageId(language) << " for \""
                                            << Eth::Cp1252ToUtf8(cp1252) << "\" (" << gaps.english
                                            << " piece(s) drawn in English)";
        }
    }
    if (memo.size() >= kMemoCap) memo.clear();
    memo.emplace(cp1252, out);
    return out;
}

bool Localization::HasTranslation(const std::string& cp1252, const Language language) const {
    if (languageNameFor(cp1252) != nullptr) return true;
    if (!HasLetters(cp1252) || language == Language::Portuguese) return true;
    if (!m_loaded) return false;
    std::string out;
    const Gaps gaps = translateNormalised(Normalise(cp1252), language, out, 0);
    return gaps.missing == 0 && gaps.english == 0;
}

std::string Localization::ImageVariant(const std::string& relativePath, const Language language) const {
    if (language == Language::Portuguese) return {};
    const std::string path = PathKey(relativePath);
    const std::string key = std::string(LanguageId(language)) + "|" + path;
    if (const auto it = m_imageResolved.find(key); it != m_imageResolved.end()) return it->second;

    std::string resolved;
    const auto existing = [](const std::filesystem::path& candidate) {
        std::error_code ec;
        return std::filesystem::is_regular_file(candidate, ec) ? candidate.generic_string() : std::string();
    };
    if (language == Language::English) {
        if (const auto it = m_images.find(path); it != m_images.end() && !it->second.empty()) {
            // As strings.json spells it, found as the disk spells it (eth/Paths.hpp).
            resolved = existing(Eth::ResolveUnder(m_dataDir, it->second));
        }
    } else {
        // E24: the language's own art, else the English (the logo keeps the
        // game's English name in the nine languages that have none of their own).
        resolved = existing(Eth::ResolveUnder(m_dataDir, std::string("images/") + LanguageId(language) + "/" + path));
        if (resolved.empty()) resolved = ImageVariant(relativePath, Language::English);
    }
    // Remembered either way: the HUD asks for every image on every frame.
    m_imageResolved.emplace(key, resolved);
    return resolved;
}

} // namespace Penumbra::Render
