// ENML, "Easy Non-Markup Language": a port of reference/eth-0.7.12/src/enml.h,
// which is what read data.enml and read and wrote hs.enml.
//
//   name { key = value; ... }        sections, both maps (order is not kept)
//
// The parser is enml.h's state machine, character for character, because its
// quirks are the format: a value runs to the next unescaped ';' across lines;
// the whitespace before a value is skipped and the whitespace after it kept;
// '/' opens a comment to the end of the line anywhere but inside a value; the
// only escapes are \; and \\ and any other backslash fails the whole parse,
// which also empties the file.

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/Log.hpp"
#include "eth/Eth.hpp"

namespace Penumbra::Eth {
namespace {

// enml.h's returnValue: a name check answers 0 (invalid), 1 (valid) or 2
// (stopped at a token), and the parser treats both non-zero answers as valid.
constexpr int kInvalid = 0;
constexpr int kValid = 1;
constexpr int kStopAtToken = 2;

enum class SeekStatus { Entity, BeginEntity, AttributeKey, Assign, ReadValue, Comment };

const char* const kErrorText[] = {
    "success",
    "expected '{' or '}'",
    "the entity name is invalid",
    "the attribute name is invalid",
    "expected '='",
    "invalid value (empty, or a backslash that escapes neither ';' nor '\\')",
};
enum ParseError { kSuccess = 0, kBracketExpected, kInvalidEntityName, kInvalidAttributeName, kAssignExpected, kInvalidValue };

// '\r' is NOT neutral (enml.h:170-173). It never reached the parser: the
// original read files through a text-mode ifstream, which folds CRLF to LF.
bool IsNeutral(const char c) { return c == ' ' || c == '\n' || c == '\t'; }

bool IsValidNameChar(const char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

// enml.h:187-205, including its order: a neutral or brace answers "stop at
// token" before a later invalid character is looked at.
int IsValidName(const string& name) {
    for (const char c : name) {
        if (IsNeutral(c) || c == '{' || c == '}') return kStopAtToken;
        if (!IsValidNameChar(c)) return kInvalid;
    }
    return kValid;
}

// enml.h:154-165: every ';' and '\' gets a backslash in front.
string FixBackslashes(const string& value) {
    string out;
    out.reserve(value.size());
    for (const char c : value) {
        if (c == ';' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

// The original's parser never saw a CR of a CRLF: every file reached it through
// getStringFromFile's text-mode read. Folding CRLF here too keeps parseString
// right if it is handed raw file bytes; text that has LF already is unchanged.
string FoldLineEnds(const string& text) {
    string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') continue;
        out += text[i];
    }
    return out;
}

// getStringFromFileC (enml.h:107-126), the read behind parseFromFile: a
// text-mode ifstream, so CRLF arrives as LF, MSVC's text mode ends the file at
// a Ctrl-Z, and the loop drops NULs. The same as GetStringFromFile's.
string TextModeRead(const string& bytes) {
    string out;
    out.reserve(bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const char c = bytes[i];
        if (c == '\x1a') break;
        if (c == '\0') continue;
        if (c == '\r' && i + 1 < bytes.size() && bytes[i + 1] == '\n') continue;
        out += c;
    }
    return out;
}

// enml.h:799-812. The cursor is left ON the name's last character, so the
// parse loop's ++ lands on the terminator and handles it. A name that runs to
// the end of the text is "" with the cursor past the end.
string ReadName(const string& text, std::size_t& cursor, const char nextValidToken, const char commentChar) {
    const std::size_t start = cursor;
    for (std::size_t t = start; t < text.size(); ++t) {
        const char c = text[t];
        if (c == commentChar || c == nextValidToken || IsNeutral(c)) {
            // start == t (a name that is empty) wraps to SIZE_MAX here and back
            // to t on the loop's ++, exactly as enml.h's unsigned cursor did.
            cursor = t - 1;
            return text.substr(start, t - start);
        }
    }
    cursor = text.size();
    return "";
}

// enml.h:817-843. "" means an invalid value (empty, unterminated, or a bad
// escape); the cursor is left on the terminating ';'.
string ReadValue(const string& text, std::size_t& cursor) {
    string value;
    for (std::size_t t = cursor; t < text.size(); ++t) {
        const char c = text[t];
        if (c == '\\') {
            const char next = t + 1 < text.size() ? text[t + 1] : '\0';
            if (next != ';' && next != '\\') return "";
            value += next;
            ++t;
        } else if (c == ';') {
            cursor = t;
            return value;
        } else {
            value += c;
        }
    }
    return "";
}

bool ReadFileBytes(const string& path, string& out) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) return false;
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

// --- Where a game path really lives ----------------------------------------

string LowerAscii(string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return text;
}

string Normalised(const string& path) {
    string generic = std::filesystem::path(path).lexically_normal().generic_string();
    while (generic.size() > 1 && generic.back() == '/') generic.pop_back();
    return generic;
}

// True when `path` is inside `root`; `relative` gets the rest ("hs.enml",
// "scenes/checkpoint.esc"). Case-insensitive, as the filesystem the original
// ran on was.
bool UnderRoot(const string& path, const string& root, string& relative) {
    if (root.empty()) return false;
    const string p = Normalised(path);
    const string r = Normalised(root);
    if (p.size() <= r.size() + 1) return false;
    if (LowerAscii(p.substr(0, r.size())) != LowerAscii(r) || p[r.size()] != '/') return false;
    relative = p.substr(r.size() + 1);
    return true;
}

// The game roots a write must never land in: the machine's, and the one the
// build was configured with (extracted/app), which a machine could differ from.
bool InsideOriginal(const string& path) {
    string ignored;
    if (UnderRoot(path, PENUMBRA_ORIGINAL_DIR, ignored)) return true;
    const Machine* machine = Machine::CurrentOrNull();
    return machine != nullptr && UnderRoot(path, machine->Config().gameRoot, ignored);
}

// A read of a game path prefers the copy the game wrote to the user directory
// (hs.enml after a new record): Machine::ReadPath, as GetStringFromFile reads.
string ResolveRead(const string& path) {
    const Machine* machine = Machine::CurrentOrNull();
    if (machine == nullptr) return path;
    const string redirected = machine->ReadPath(path);
    return redirected.empty() ? path : redirected;
}

// Where a write of `path` goes: a game path (relative, or absolute under the
// game root) to the user directory, "" when there is none; with no machine a
// game path is not written at all.
string ResolveWrite(const string& path) {
    const Machine* machine = Machine::CurrentOrNull();
    if (machine != nullptr) return machine->IsGamePath(path) ? machine->WritePath(path) : path;
    string ignored;
    const bool gamePath = !std::filesystem::path(path).is_absolute() || UnderRoot(path, PENUMBRA_ORIGINAL_DIR, ignored);
    return gamePath ? string() : path;
}

} // namespace

// --- enmlEntity ------------------------------------------------------------

void enmlEntity::add(const string& name, const string& value) {
    // enml.h:251-256 asserts a valid name and a non-empty value in debug
    // builds only; the shipped game stored whatever it was given.
    const auto inserted = m_attributes.insert_or_assign(name, value);
    if (inserted.second) m_order.push_back(name);
}

string enmlEntity::get(const string& name) const {
    const auto it = m_attributes.find(name);
    return it != m_attributes.end() ? it->second : string();
}

// --- enmlFile --------------------------------------------------------------

uint enmlFile::parseString(const string& text) {
    // Returns 0 on success and the line of the error otherwise, as enml.h's
    // parseString (:461-584) did; the scripts ignore it (main.as:136,
    // scores.as:50/97/115).
    const string str = FoldLineEnds(text);
    m_entities.clear();
    m_order.clear();

    SeekStatus status = SeekStatus::Entity;
    SeekStatus lastStatus = status;
    uint line = 1;
    string entityName;
    string keyName;
    enmlEntity entity;

    const auto fail = [&](const ParseError error) {
        m_entities.clear();
        m_order.clear();
        SUPERSONIC_LOG_WARN("Penumbra") << "enml: parse error at line " << line << ": " << kErrorText[error];
        return line;
    };

    for (std::size_t cursor = 0; cursor < str.size(); ++cursor) {
        if (str[cursor] == '\n') {
            ++line;
            if (status == SeekStatus::Comment) status = lastStatus;
        } else if (status != SeekStatus::ReadValue && status != SeekStatus::Comment && str[cursor] == '/') {
            lastStatus = status;
            status = SeekStatus::Comment;
        }

        switch (status) {
        case SeekStatus::Comment:
            break;
        case SeekStatus::Entity:
            if (!IsNeutral(str[cursor])) {
                entityName = ReadName(str, cursor, '{', '/');
                if (IsValidName(entityName) == kInvalid) return fail(kInvalidEntityName);
                status = SeekStatus::BeginEntity;
            }
            break;
        case SeekStatus::BeginEntity:
            if (!IsNeutral(str[cursor])) {
                if (str[cursor] != '{') return fail(kBracketExpected);
                status = SeekStatus::AttributeKey;
                entity.clear();
            }
            break;
        case SeekStatus::AttributeKey:
            if (!IsNeutral(str[cursor])) {
                if (str[cursor] == '}') {
                    addEntity(entityName, entity);
                    entityName.clear();
                    status = SeekStatus::Entity;
                } else {
                    keyName = ReadName(str, cursor, '=', '/');
                    if (IsValidName(keyName) == kInvalid) return fail(kInvalidAttributeName);
                    status = SeekStatus::Assign;
                }
            }
            break;
        case SeekStatus::Assign:
            if (!IsNeutral(str[cursor])) {
                if (str[cursor] != '=') return fail(kAssignExpected);
                status = SeekStatus::ReadValue;
            }
            break;
        case SeekStatus::ReadValue:
            if (!IsNeutral(str[cursor])) {
                const string value = ReadValue(str, cursor);
                if (value.empty()) return fail(kInvalidValue);
                status = SeekStatus::AttributeKey;
                entity.add(keyName, value);
            }
            break;
        }
    }
    // A section still open at the end is dropped without an error, as in
    // enml.h: only '}' adds it.
    return kSuccess;
}

bool enmlFile::parseFromFile(const string& absolutePath) {
    // enml.h:778-782 over getStringFromFile, which answers "" for a missing
    // file - so a missing file parses as an empty one and returns true.
    string bytes;
    ReadFileBytes(ResolveRead(absolutePath), bytes);
    return parseString(TextModeRead(bytes)) == kSuccess;
}

bool enmlFile::exists(const string& entity) const {
    return m_entities.count(entity) != 0;
}

string enmlFile::get(const string& entity, const string& attribute) const {
    const auto it = m_entities.find(entity);
    return it != m_entities.end() ? it->second.get(attribute) : string();
}

// The typed getters (enml.h:646-709) scan with sscanf and touch `out` only when
// the scan succeeded: a missing key (data.enml's global.lv20) leaves the
// caller's value as it was, which addToExp depends on (util.as:406-418).
#ifdef _MSC_VER
#define PENUMBRA_ENML_SSCANF sscanf_s
#else
#define PENUMBRA_ENML_SSCANF std::sscanf
#endif

bool enmlFile::getInt(const string& entity, const string& attribute, int& out) const {
    const string str = get(entity, attribute);
    if (str.empty()) return false;
    int value = 0;
    if (PENUMBRA_ENML_SSCANF(str.c_str(), "%d", &value) < 1) return false;
    out = value;
    return true;
}

bool enmlFile::getUint(const string& entity, const string& attribute, uint& out) const {
    const string str = get(entity, attribute);
    if (str.empty()) return false;
    unsigned int value = 0;
    if (PENUMBRA_ENML_SSCANF(str.c_str(), "%u", &value) < 1) return false;
    out = value;
    return true;
}

bool enmlFile::getFloat(const string& entity, const string& attribute, float& out) const {
    const string str = get(entity, attribute);
    if (str.empty()) return false;
    float value = 0.0f;
    if (PENUMBRA_ENML_SSCANF(str.c_str(), "%f", &value) < 1) return false;
    out = value;
    return true;
}

bool enmlFile::getDouble(const string& entity, const string& attribute, double& out) const {
    const string str = get(entity, attribute);
    if (str.empty()) return false;
    double value = 0.0;
    if (PENUMBRA_ENML_SSCANF(str.c_str(), "%lf", &value) < 1) return false;
    out = value;
    return true;
}

#undef PENUMBRA_ENML_SSCANF

void enmlFile::addEntity(const string& name, const enmlEntity& entity) {
    // enml.h:388-400: an existing section is emptied and refilled; a new one
    // only comes into being with its first attribute, so adding an empty
    // entity under a new name adds nothing.
    const auto existing = m_entities.find(name);
    if (existing != m_entities.end()) existing->second.clear();
    for (const string& key : entity.Order()) {
        auto it = m_entities.find(name);
        if (it == m_entities.end()) {
            it = m_entities.emplace(name, enmlEntity()).first;
            m_order.push_back(name);
        }
        it->second.add(key, entity.get(key));
    }
}

string enmlFile::generateString() const {
    // enml.h:405-420, with std::endl as '\n': sections and keys in map order.
    // Keys are sorted here because enmlEntity keeps its map private; std::string's
    // operator< is the map's own ordering.
    string out;
    for (const auto& [name, entity] : m_entities) {
        out += name;
        out += "\n{\n";
        std::vector<string> keys = entity.Order();
        std::sort(keys.begin(), keys.end());
        for (const string& key : keys) {
            out += '\t';
            out += key;
            out += " = ";
            out += FixBackslashes(entity.get(key));
            out += ";\n";
        }
        out += "}\n\n";
    }
    return out;
}

void enmlFile::writeToFile(const string& absolutePath) const {
    // The original wrote over its own install folder (scores.as:88 writes
    // GetAbsolutePath("hs.enml")). A game path goes to the user directory
    // instead; with none configured nothing is written. extracted/ is never
    // written, whatever the path.
    const string target = ResolveWrite(absolutePath);
    if (target.empty()) {
        SUPERSONIC_LOG_WARN("Penumbra") << "enml: no user directory, " << absolutePath << " not written";
        return;
    }
    if (InsideOriginal(target)) {
        SUPERSONIC_LOG_ERROR("Penumbra") << "enml: refusing to write into the original's folder: " << target;
        return;
    }

    // saveStringToFile used a text-mode ofstream (enml.h:135-145), so every
    // '\n' reached the disk as CRLF - which is the shipped hs.enml's layout.
    const string text = generateString();
    string bytes;
    bytes.reserve(text.size() + text.size() / 8);
    for (const char c : text) {
        if (c == '\n') bytes += '\r';
        bytes += c;
    }

    const std::filesystem::path path(target);
    std::error_code ignored;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ignored);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        SUPERSONIC_LOG_WARN("Penumbra") << "enml: cannot write " << target;
        return;
    }
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

} // namespace Penumbra::Eth
