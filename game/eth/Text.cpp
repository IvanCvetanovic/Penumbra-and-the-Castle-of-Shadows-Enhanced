// The cp1252 edges of the port, and the text AngelScript made of numbers.
//
// The original's strings stay cp1252 bytes everywhere (CLAUDE.md rule 8); these
// are the only places they turn into Unicode: the log, and whatever the
// renderer asks for.

#include "eth/Text.hpp"

#include <locale>
#include <sstream>

#include "core/Log.hpp"
#include "eth/Eth.hpp"

namespace Penumbra::Eth {
namespace {

// Windows-1252 0x80..0x9F. The rest of the high half is Latin-1, code point =
// byte. Zero marks the four bytes cp1252 leaves undefined that the port does
// not use; the fifth, 0x8D, is the port's U+0107 (E21, Text.hpp).
constexpr unsigned kCp1252High[32] = {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, kCAcuteCodePoint, 0x017D, 0,
    0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178,
};
static_assert(kCp1252High[kCAcuteByte - 0x80u] == kCAcuteCodePoint);

constexpr unsigned kReplacement = 0xFFFDu;

void AppendUtf8(string& out, const unsigned cp) {
    if (cp < 0x80u) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800u) {
        out += static_cast<char>(0xC0u | (cp >> 6));
        out += static_cast<char>(0x80u | (cp & 0x3Fu));
    } else if (cp < 0x10000u) {
        out += static_cast<char>(0xE0u | (cp >> 12));
        out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
        out += static_cast<char>(0x80u | (cp & 0x3Fu));
    } else {
        out += static_cast<char>(0xF0u | (cp >> 18));
        out += static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
        out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
        out += static_cast<char>(0x80u | (cp & 0x3Fu));
    }
}

char ToCp1252Byte(const unsigned cp) {
    // U+0080..U+009F are C1 controls, not the cp1252 glyphs at those bytes, so
    // they are among the characters cp1252 cannot hold.
    if (cp < 0x80u || (cp >= 0xA0u && cp <= 0xFFu)) return static_cast<char>(cp);
    for (unsigned i = 0; i < 32; ++i) {
        if (kCp1252High[i] == cp) return static_cast<char>(0x80u + i);
    }
    return '?';
}

// AngelScript's string + number went through an ostringstream with its default
// precision of 6 (reference/eth-0.7.12/src/addons/scriptstdstring.cpp, e.g.
// AddStringDouble). The classic locale keeps the decimal point a point whatever
// the process locale is.
template <typename T>
string Streamed(const T& value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << value;
    return stream.str();
}

} // namespace

unsigned Cp1252CodePoint(const unsigned char byte) {
    if (byte < 0x80u || byte >= 0xA0u) return byte;
    const unsigned cp = kCp1252High[byte - 0x80u];
    return cp != 0 ? cp : kReplacement;
}

string Cp1252ToUtf8(const string& text) {
    string out;
    out.reserve(text.size() + text.size() / 4);
    for (const char c : text) AppendUtf8(out, Cp1252CodePoint(static_cast<unsigned char>(c)));
    return out;
}

string Utf8ToCp1252(const string& text) {
    string out;
    out.reserve(text.size());
    const std::size_t size = text.size();
    std::size_t i = 0;
    while (i < size) {
        const unsigned lead = static_cast<unsigned char>(text[i]);
        if (lead < 0x80u) {
            out += static_cast<char>(lead);
            ++i;
            continue;
        }
        std::size_t length = 0;
        unsigned cp = 0;
        unsigned minimum = 0;
        if ((lead & 0xE0u) == 0xC0u) {
            length = 2; cp = lead & 0x1Fu; minimum = 0x80u;
        } else if ((lead & 0xF0u) == 0xE0u) {
            length = 3; cp = lead & 0x0Fu; minimum = 0x800u;
        } else if ((lead & 0xF8u) == 0xF0u) {
            length = 4; cp = lead & 0x07u; minimum = 0x10000u;
        } else {
            out += '?';     // a stray continuation byte or an invalid lead
            ++i;
            continue;
        }
        bool complete = i + length <= size;
        for (std::size_t k = 1; complete && k < length; ++k) {
            const unsigned next = static_cast<unsigned char>(text[i + k]);
            if ((next & 0xC0u) != 0x80u) {
                complete = false;
            } else {
                cp = (cp << 6) | (next & 0x3Fu);
            }
        }
        if (!complete) {
            // A truncated sequence costs one '?' and resynchronises on the next
            // byte, so the text that follows it survives.
            out += '?';
            ++i;
            continue;
        }
        const bool valid = cp >= minimum && cp <= 0x10FFFFu && (cp < 0xD800u || cp > 0xDFFFu);
        out += valid ? ToCp1252Byte(cp) : '?';
        i += length;
    }
    return out;
}

string Str(const int value) { return Streamed(value); }
string Str(const uint value) { return Streamed(value); }
// A float reached the string add-on as a double (opAdd(double) is the only
// floating overload registered), so it is formatted as that double.
string Str(const float value) { return Streamed(static_cast<double>(value)); }
string Str(const double value) { return Streamed(value); }

// NOT "true"/"false": 0.7.12's add-on writes `stream << b ? "true" : "false";`,
// which parses as `(stream << b) ? ...` and streams the bool as a number
// (scriptstdstring.cpp AssignBoolToString/AddStringBool/AddBoolString, the
// native registrations an x86 build uses).
string Str(const bool value) { return value ? "1" : "0"; }

// Not an AngelScript conversion (scriptmath2d registers none); a convenience
// for the port's own diagnostics.
string Str(const vector2& value) { return "(" + Str(value.x) + ", " + Str(value.y) + ")"; }

void print(const string& text) {
    SUPERSONIC_LOG_INFO("Penumbra") << Cp1252ToUtf8(text);
}

} // namespace Penumbra::Eth
