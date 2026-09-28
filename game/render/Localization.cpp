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
// brackets and the 0x95 bullet are the same in both languages, so the timer,
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
    // Image variants are named relative to the file they are listed in.
    m_dataDir = std::filesystem::path(path).parent_path().generic_string();
    return true;
}

bool Localization::LoadFromJson(const std::string& utf8Json, std::string& error) {
    m_strings.clear();
    m_patterns.clear();
    m_images.clear();
    m_memo.clear();
    m_imageResolved.clear();
    m_loaded = false;

    // A BOM is what an editor on Windows may leave; the parser would call it
    // content before the document.
    const std::string_view bom = "\xEF\xBB\xBF";
    const std::string text = utf8Json.compare(0, bom.size(), bom) == 0 ? utf8Json.substr(bom.size()) : utf8Json;

    Supersonic::Json::Value root;
    if (!Supersonic::Json::Parse(text, root, error)) return false;
    if (!root.IsObject()) {
        error = "the document is not an object";
        return false;
    }

    // Keys are trimmed as well as normalised, because lookup trims the text and
    // puts its own surrounding whitespace back around the English.
    for (const auto& [key, value] : root["strings"].AsObject()) {
        if (key.empty() || key[0] == '_' || !value.IsString()) continue;   // "_..." are notes
        m_strings[Trimmed(Normalise(Eth::Utf8ToCp1252(key)))] =
            Trimmed(Normalise(Eth::Utf8ToCp1252(value.AsString())));
    }
    for (const Supersonic::Json::Value& entry : root["patterns"].AsArray()) {
        const std::string pt = entry["pt"].AsString();
        const std::string en = entry["en"].AsString();
        if (pt.empty()) continue;
        m_patterns.push_back(ParsePattern(Trimmed(Normalise(Eth::Utf8ToCp1252(pt))),
                                          Trimmed(Normalise(Eth::Utf8ToCp1252(en)))));
    }
    for (const auto& [path, entry] : root["images"].AsObject()) {
        if (path.empty() || path[0] == '_') continue;
        m_images[PathKey(path)] = entry["en"].AsString();
    }
    m_loaded = true;
    return true;
}

Localization::Pattern Localization::ParsePattern(const std::string& pt, const std::string& en) {
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

    std::string text;
    const auto flushOut = [&]() {
        if (text.empty()) return;
        OutToken token;
        token.literal = std::move(text);
        pattern.en.push_back(std::move(token));
        text.clear();
    };
    for (std::size_t i = 0; i < en.size(); ++i) {
        if (en[i] == '{' && i + 2 < en.size() && en[i + 1] >= '1' && en[i + 1] <= '9' && en[i + 2] == '}') {
            flushOut();
            OutToken token;
            token.capture = en[i + 1] - '1';
            pattern.en.push_back(token);
            i += 2;
            continue;
        }
        text += en[i];
    }
    flushOut();
    return pattern;
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

int Localization::translateNormalised(const std::string& text, std::string& out, const int depth) const {
    if (depth > kMaxDepth) {
        out = text;
        return 1;
    }
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && IsSpace(text[begin])) ++begin;
    while (end > begin && IsSpace(text[end - 1])) --end;
    if (begin == end) {
        out = text;
        return 0;
    }
    std::string core;
    const int missing = translateCore(text.substr(begin, end - begin), core, depth);
    out = text.substr(0, begin) + core + text.substr(end);
    return missing;
}

int Localization::translateCore(const std::string& core, std::string& out, const int depth) const {
    if (!HasLetters(core)) {
        out = core;
        return 0;
    }
    if (const auto it = m_strings.find(core); it != m_strings.end()) {
        out = it->second;
        return 0;
    }

    std::vector<std::string> captures;
    for (const Pattern& pattern : m_patterns) {
        if (!matchPattern(pattern, core, captures)) continue;
        int missing = 0;
        out.clear();
        for (const OutToken& token : pattern.en) {
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
                missing += translateNormalised(captures[index], piece, depth + 1);
                out += piece;
            } else {
                out += captures[index];
            }
        }
        return missing;
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
        int missing = 0;
        out.clear();
        for (std::size_t i = 0; i < pieces.size(); ++i) {
            std::string piece;
            missing += translateNormalised(pieces[i], piece, depth + 1);
            out += piece;
            if (i < breaks.size()) out += breaks[i];
        }
        return missing;
    }

    out = core;
    return 1;
}

std::string Localization::Translate(const std::string& cp1252, const Language language) const {
    if (language == Language::Portuguese || !m_loaded || !HasLetters(cp1252)) return cp1252;
    if (const auto it = m_memo.find(cp1252); it != m_memo.end()) return it->second;

    std::string out;
    const int missing = translateNormalised(Normalise(cp1252), out, 0);
    if (missing > 0 && m_logged.size() < kLogCap && m_logged.insert(cp1252).second) {
        SUPERSONIC_LOG_WARN("Penumbra") << "localization: no English for \"" << Eth::Cp1252ToUtf8(cp1252)
                                        << "\" (" << missing << " piece(s) left in Portuguese)";
    }
    if (m_memo.size() >= kMemoCap) m_memo.clear();
    m_memo.emplace(cp1252, out);
    return out;
}

bool Localization::HasTranslation(const std::string& cp1252) const {
    if (!HasLetters(cp1252)) return true;
    if (!m_loaded) return false;
    std::string out;
    return translateNormalised(Normalise(cp1252), out, 0) == 0;
}

std::string Localization::ImageVariant(const std::string& relativePath, const Language language) const {
    if (language == Language::Portuguese || m_images.empty()) return {};
    const std::string key = PathKey(relativePath);
    if (const auto it = m_imageResolved.find(key); it != m_imageResolved.end()) return it->second;

    std::string resolved;
    if (const auto it = m_images.find(key); it != m_images.end() && !it->second.empty()) {
        // As strings.json spells it, found as the disk spells it (eth/Paths.hpp).
        const std::filesystem::path candidate(Eth::ResolveUnder(m_dataDir, it->second));
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) resolved = candidate.generic_string();
    }
    // Remembered either way: the HUD asks for every image on every frame.
    m_imageResolved.emplace(key, resolved);
    return resolved;
}

} // namespace Penumbra::Render
