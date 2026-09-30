#include "render/ArabicShaping.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

#include "eth/Text.hpp"

namespace Penumbra::Render {
namespace {

bool In(const char32_t codePoint, const unsigned first, const unsigned last) {
    const auto value = static_cast<unsigned>(codePoint);
    return value >= first && value <= last;
}

// ---- shaping ----------------------------------------------------------------------------

// Unicode's joining types (ArabicShaping.txt) of what the translations use.
enum class Join { U, R, D, C, T };

Join JoiningOf(const char32_t c) {
    if (In(c, 0x064B, 0x065F) || c == 0x0670 || In(c, 0x06D6, 0x06DC) || In(c, 0x06DF, 0x06E4) ||
        In(c, 0x06E7, 0x06E8) || In(c, 0x06EA, 0x06ED)) {
        return Join::T;   // the harakat and Quranic marks
    }
    if (c == 0x0640) return Join::C;   // tatweel
    if (In(c, 0x0622, 0x0625) || c == 0x0627 || c == 0x0629 || In(c, 0x062F, 0x0632) || c == 0x0648 ||
        In(c, 0x0671, 0x0673) || In(c, 0x0675, 0x0677) || In(c, 0x0688, 0x0699) || c == 0x06C0 ||
        In(c, 0x06C3, 0x06CB) || c == 0x06CD || c == 0x06CF || In(c, 0x06D2, 0x06D3) || c == 0x06D5 ||
        In(c, 0x06EE, 0x06EF)) {
        return Join::R;
    }
    if (c == 0x0626 || c == 0x0628 || In(c, 0x062A, 0x062E) || In(c, 0x0633, 0x063F) || In(c, 0x0641, 0x0647) ||
        In(c, 0x0649, 0x064A) || In(c, 0x066E, 0x066F) || In(c, 0x0678, 0x0687) || In(c, 0x069A, 0x06BF) ||
        In(c, 0x06C1, 0x06C2) || c == 0x06CC || c == 0x06CE || In(c, 0x06D0, 0x06D1) || In(c, 0x06FA, 0x06FC) ||
        c == 0x06FF) {
        return Join::D;
    }
    return Join::U;
}

// Presentation Forms-B for U+0621..U+064A: isolated, final, initial, medial
// (0: the letter has no such form). Alef maksura's initial and medial are
// in Forms-A.
struct Forms {
    char32_t isolated;
    char32_t final;
    char32_t initial;
    char32_t medial;
};
constexpr Forms kForms[] = {
    {0xFE80, 0, 0, 0},                    // 0621 hamza
    {0xFE81, 0xFE82, 0, 0},               // 0622 alef with madda above
    {0xFE83, 0xFE84, 0, 0},               // 0623 alef with hamza above
    {0xFE85, 0xFE86, 0, 0},               // 0624 waw with hamza above
    {0xFE87, 0xFE88, 0, 0},               // 0625 alef with hamza below
    {0xFE89, 0xFE8A, 0xFE8B, 0xFE8C},     // 0626 yeh with hamza above
    {0xFE8D, 0xFE8E, 0, 0},               // 0627 alef
    {0xFE8F, 0xFE90, 0xFE91, 0xFE92},     // 0628 beh
    {0xFE93, 0xFE94, 0, 0},               // 0629 teh marbuta
    {0xFE95, 0xFE96, 0xFE97, 0xFE98},     // 062A teh
    {0xFE99, 0xFE9A, 0xFE9B, 0xFE9C},     // 062B theh
    {0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0},     // 062C jeem
    {0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4},     // 062D hah
    {0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8},     // 062E khah
    {0xFEA9, 0xFEAA, 0, 0},               // 062F dal
    {0xFEAB, 0xFEAC, 0, 0},               // 0630 thal
    {0xFEAD, 0xFEAE, 0, 0},               // 0631 reh
    {0xFEAF, 0xFEB0, 0, 0},               // 0632 zain
    {0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4},     // 0633 seen
    {0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8},     // 0634 sheen
    {0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC},     // 0635 sad
    {0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0},     // 0636 dad
    {0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4},     // 0637 tah
    {0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8},     // 0638 zah
    {0xFEC9, 0xFECA, 0xFECB, 0xFECC},     // 0639 ain
    {0xFECD, 0xFECE, 0xFECF, 0xFED0},     // 063A ghain
    {0, 0, 0, 0},                         // 063B-063F: not in Forms-B
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},                         // 0640 tatweel: joins, has no forms
    {0xFED1, 0xFED2, 0xFED3, 0xFED4},     // 0641 feh
    {0xFED5, 0xFED6, 0xFED7, 0xFED8},     // 0642 qaf
    {0xFED9, 0xFEDA, 0xFEDB, 0xFEDC},     // 0643 kaf
    {0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0},     // 0644 lam
    {0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4},     // 0645 meem
    {0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8},     // 0646 noon
    {0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC},     // 0647 heh
    {0xFEED, 0xFEEE, 0, 0},               // 0648 waw
    {0xFEEF, 0xFEF0, 0xFBE8, 0xFBE9},     // 0649 alef maksura
    {0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4},     // 064A yeh
};
static_assert(sizeof(kForms) / sizeof(kForms[0]) == 0x064A - 0x0621 + 1, "one entry per letter");

// Lam + this alef -> the ligature's isolated form (its final form is one more).
char32_t LamAlef(const char32_t alef) {
    switch (alef) {
        case 0x0622: return 0xFEF5;
        case 0x0623: return 0xFEF7;
        case 0x0625: return 0xFEF9;
        case 0x0627: return 0xFEFB;
        default: return 0;
    }
}

// ---- direction --------------------------------------------------------------------------

// UAX #9's bidirectional types, as far as the translations need them.
enum class Bidi { L, R, AL, EN, AN, ES, CS, ET, NSM, WS, S, ON };

Bidi BidiOf(const char32_t c) {
    if (In(c, '0', '9') || In(c, 0x06F0, 0x06F9) || In(c, 0xFF10, 0xFF19)) return Bidi::EN;
    if (In(c, 0x0660, 0x0669) || In(c, 0x066B, 0x066C)) return Bidi::AN;
    if (c == '+' || c == '-' || c == 0x2212) return Bidi::ES;
    if (c == ',' || c == '.' || c == ':' || c == '/' || c == 0x00A0 || c == 0x060C || c == 0x202F) return Bidi::CS;
    if (c == '#' || c == '$' || c == '%' || In(c, 0x00A2, 0x00A5) || In(c, 0x00B0, 0x00B1) || c == 0x066A ||
        In(c, 0x2030, 0x2034) || In(c, 0x20A0, 0x20CF)) {
        return Bidi::ET;
    }
    if (c == ' ' || In(c, 0x2000, 0x200A) || c == 0x3000) return Bidi::WS;
    if (c == '\t') return Bidi::S;
    if (In(c, 0x0300, 0x036F) || JoiningOf(c) == Join::T) return Bidi::NSM;
    if (In(c, 0x0590, 0x05FF)) return Bidi::R;
    if (ArabicShaping::IsArabic(c) && c != 0xFEFF) return Bidi::AL;
    if (In(c, 'A', 'Z') || In(c, 'a', 'z') || c == 0x00AA || c == 0x00B5 || c == 0x00BA ||
        (In(c, 0x00C0, 0x02B8) && c != 0x00D7 && c != 0x00F7) || In(c, 0x0370, 0x058F) || In(c, 0x1E00, 0x1FFF) ||
        In(c, 0x3005, 0x3007) || In(c, 0x3041, 0x3096) || In(c, 0x309D, 0x30FA) || In(c, 0x30FC, 0x30FF) ||
        In(c, 0x3400, 0x4DBF) || In(c, 0x4E00, 0x9FFF) || In(c, 0xF900, 0xFAFF) || In(c, 0xFF21, 0xFF3A) ||
        In(c, 0xFF41, 0xFF5A) || In(c, 0xFF66, 0xFFDC)) {
        return Bidi::L;
    }
    return Bidi::ON;
}

// The bracket pairs UAX #9 pairs and mirrors (BidiBrackets.txt, BidiMirroring.txt),
// as far as the translations go: 0 when `c` is not one.
char32_t Mirror(const char32_t c) {
    switch (c) {
        case '(': return ')';
        case ')': return '(';
        case '[': return ']';
        case ']': return '[';
        case '{': return '}';
        case '}': return '{';
        case '<': return '>';
        case '>': return '<';
        case 0x00AB: return 0x00BB;
        case 0x00BB: return 0x00AB;
        case 0x2039: return 0x203A;
        case 0x203A: return 0x2039;
        default: return 0;
    }
}

bool Opens(const char32_t c) { return c == '(' || c == '[' || c == '{' || c == 0x00AB || c == 0x2039; }
bool Closes(const char32_t c) { return c == ')' || c == ']' || c == '}' || c == 0x00BB || c == 0x203A; }

// How a resolved type counts for the neutrals and brackets around it: numbers
// as right to left (UAX #9 N1), anything not strong as nothing.
enum class Strong { None, L, R };
Strong StrongOf(const Bidi type) {
    if (type == Bidi::L) return Strong::L;
    if (type == Bidi::R || type == Bidi::EN || type == Bidi::AN) return Strong::R;
    return Strong::None;
}

// One line, shaped already: the order to draw it in.
std::u32string Reorder(const std::u32string& line, const bool rightToLeft) {
    const std::size_t n = line.size();
    const Bidi sos = rightToLeft ? Bidi::R : Bidi::L;
    const Strong embedding = rightToLeft ? Strong::R : Strong::L;
    std::vector<Bidi> types(n);
    for (std::size_t i = 0; i < n; ++i) types[i] = BidiOf(line[i]);

    // W1: a mark takes the type of what it follows.
    for (std::size_t i = 0; i < n; ++i) {
        if (types[i] == Bidi::NSM) types[i] = i == 0 ? sos : types[i - 1];
    }
    // W2: a European digit after Arabic letters is an Arabic number; W3: AL is R.
    Bidi lastStrong = sos;
    for (std::size_t i = 0; i < n; ++i) {
        if (types[i] == Bidi::L || types[i] == Bidi::R || types[i] == Bidi::AL) lastStrong = types[i];
        if (types[i] == Bidi::EN && lastStrong == Bidi::AL) types[i] = Bidi::AN;
    }
    for (Bidi& type : types) {
        if (type == Bidi::AL) type = Bidi::R;
    }
    // W4: one separator between two numbers of a kind joins them ("12:05", "1.5").
    for (std::size_t i = 1; i + 1 < n; ++i) {
        const Bidi before = types[i - 1];
        const Bidi after = types[i + 1];
        if (types[i] == Bidi::ES && before == Bidi::EN && after == Bidi::EN) types[i] = Bidi::EN;
        else if (types[i] == Bidi::CS && before == Bidi::EN && after == Bidi::EN) types[i] = Bidi::EN;
        else if (types[i] == Bidi::CS && before == Bidi::AN && after == Bidi::AN) types[i] = Bidi::AN;
    }
    // W5: terminators next to a European number are part of it ("50%").
    for (std::size_t i = 0; i < n;) {
        if (types[i] != Bidi::ET) {
            ++i;
            continue;
        }
        std::size_t end = i;
        while (end < n && types[end] == Bidi::ET) ++end;
        const bool number = (i > 0 && types[i - 1] == Bidi::EN) || (end < n && types[end] == Bidi::EN);
        if (number) {
            std::fill(types.begin() + static_cast<std::ptrdiff_t>(i), types.begin() + static_cast<std::ptrdiff_t>(end),
                      Bidi::EN);
        }
        i = end;
    }
    // W6: the separators and terminators left are neutral.
    for (Bidi& type : types) {
        if (type == Bidi::ES || type == Bidi::CS || type == Bidi::ET) type = Bidi::ON;
    }
    // W7: a European number in left-to-right text is left to right.
    lastStrong = sos;
    for (std::size_t i = 0; i < n; ++i) {
        if (types[i] == Bidi::L || types[i] == Bidi::R) lastStrong = types[i];
        if (types[i] == Bidi::EN && lastStrong == Bidi::L) types[i] = Bidi::L;
    }

    // N0: a bracket pair takes the embedding direction when what it holds has
    // it; the other direction when that is all it holds and its context is
    // that direction too; else the embedding direction. Pairs in the order
    // they open, each seeing what the ones before it resolved.
    std::vector<std::pair<std::size_t, std::size_t>> pairs;
    std::vector<std::size_t> open;
    for (std::size_t i = 0; i < n; ++i) {
        if (types[i] != Bidi::ON) continue;
        if (Opens(line[i])) {
            if (open.size() < 63) open.push_back(i);
        } else if (Closes(line[i])) {
            for (std::size_t k = open.size(); k-- > 0;) {
                if (Mirror(line[open[k]]) == line[i]) {
                    pairs.emplace_back(open[k], i);
                    open.resize(k);
                    break;
                }
            }
        }
    }
    std::sort(pairs.begin(), pairs.end());
    for (const auto& [first, last] : pairs) {
        bool sameAsEmbedding = false;
        bool opposite = false;
        for (std::size_t i = first + 1; i < last; ++i) {
            const Strong strong = StrongOf(types[i]);
            if (strong == embedding) sameAsEmbedding = true;
            else if (strong != Strong::None) opposite = true;
        }
        Strong chosen = Strong::None;
        if (sameAsEmbedding) {
            chosen = embedding;
        } else if (opposite) {
            Strong context = embedding;
            for (std::size_t i = first; i-- > 0;) {
                if (const Strong strong = StrongOf(types[i]); strong != Strong::None) {
                    context = strong;
                    break;
                }
            }
            chosen = context != embedding ? context : embedding;
        }
        if (chosen != Strong::None) {
            types[first] = types[last] = chosen == Strong::L ? Bidi::L : Bidi::R;
        }
    }

    // N1 and N2: a run of neutrals between two strong types of one direction
    // takes it; any other, the paragraph's.
    for (std::size_t i = 0; i < n;) {
        const bool neutral = types[i] == Bidi::WS || types[i] == Bidi::S || types[i] == Bidi::ON;
        if (!neutral) {
            ++i;
            continue;
        }
        std::size_t end = i;
        while (end < n && (types[end] == Bidi::WS || types[end] == Bidi::S || types[end] == Bidi::ON)) ++end;
        const Strong before = i == 0 ? embedding : StrongOf(types[i - 1]);
        const Strong after = end == n ? embedding : StrongOf(types[end]);
        const Strong chosen = before == after && before != Strong::None ? before : embedding;
        std::fill(types.begin() + static_cast<std::ptrdiff_t>(i), types.begin() + static_cast<std::ptrdiff_t>(end),
                  chosen == Strong::L ? Bidi::L : Bidi::R);
        i = end;
    }

    // Levels (I1, I2), with the line's trailing white space at the
    // paragraph's (L1).
    const int base = rightToLeft ? 1 : 0;
    std::vector<int> levels(n);
    for (std::size_t i = 0; i < n; ++i) {
        const Bidi type = types[i];
        if (base == 0) levels[i] = type == Bidi::R ? 1 : (type == Bidi::EN || type == Bidi::AN) ? 2 : 0;
        else levels[i] = type == Bidi::R ? 1 : 2;
    }
    for (std::size_t i = n; i-- > 0;) {
        const Bidi original = BidiOf(line[i]);
        if (original != Bidi::WS && original != Bidi::S) break;
        levels[i] = base;
    }

    // L2: from the highest level to the lowest odd one, every run at that
    // level or above reversed; then L4: what reads right to left mirrored.
    std::u32string out = line;
    for (std::size_t i = 0; i < n; ++i) {
        if ((levels[i] & 1) != 0) {
            if (const char32_t mirrored = Mirror(out[i]); mirrored != 0) out[i] = mirrored;
        }
    }
    const int highest = n == 0 ? 0 : *std::max_element(levels.begin(), levels.end());
    for (int level = highest; level >= 1; --level) {
        for (std::size_t i = 0; i < n;) {
            if (levels[i] < level) {
                ++i;
                continue;
            }
            std::size_t end = i;
            while (end < n && levels[end] >= level) ++end;
            std::reverse(out.begin() + static_cast<std::ptrdiff_t>(i), out.begin() + static_cast<std::ptrdiff_t>(end));
            std::reverse(levels.begin() + static_cast<std::ptrdiff_t>(i),
                         levels.begin() + static_cast<std::ptrdiff_t>(end));
            i = end;
        }
    }
    return out;
}

} // namespace

namespace ArabicShaping {

bool IsArabic(const char32_t codePoint) {
    return In(codePoint, 0x0600, 0x06FF) || In(codePoint, 0x0750, 0x077F) || In(codePoint, 0xFB50, 0xFDFF) ||
           In(codePoint, 0xFE70, 0xFEFF);
}

std::u32string Shape(const std::u32string& logical) {
    const std::size_t n = logical.size();
    std::vector<Join> joins(n);
    bool any = false;
    for (std::size_t i = 0; i < n; ++i) {
        joins[i] = JoiningOf(logical[i]);
        any = any || joins[i] == Join::R || joins[i] == Join::D;
    }
    if (!any) return logical;

    // The nearest neighbour that is not transparent, either way.
    const auto neighbour = [&](const std::size_t i, const int step) -> std::ptrdiff_t {
        for (auto k = static_cast<std::ptrdiff_t>(i) + step; k >= 0 && k < static_cast<std::ptrdiff_t>(n); k += step) {
            if (joins[static_cast<std::size_t>(k)] != Join::T) return k;
        }
        return -1;
    };
    std::u32string out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const char32_t c = logical[i];
        const Join join = joins[i];
        if (join != Join::R && join != Join::D) {
            out += c;
            continue;
        }
        const std::ptrdiff_t before = neighbour(i, -1);
        const std::ptrdiff_t after = neighbour(i, 1);
        const bool joinsBefore = before >= 0 && (joins[static_cast<std::size_t>(before)] == Join::D ||
                                                 joins[static_cast<std::size_t>(before)] == Join::C);
        const bool joinsAfter = join == Join::D && after >= 0 && joins[static_cast<std::size_t>(after)] != Join::U &&
                                joins[static_cast<std::size_t>(after)] != Join::T;

        // Lam-alef: one glyph, isolated or final as the lam joins before it.
        if (c == 0x0644 && i + 1 < n) {
            if (const char32_t ligature = LamAlef(logical[i + 1]); ligature != 0) {
                out += joinsBefore ? static_cast<char32_t>(ligature + 1) : ligature;
                ++i;
                continue;
            }
        }
        if (!In(c, 0x0621, 0x064A)) {
            out += c;
            continue;
        }
        const Forms& forms = kForms[c - 0x0621];
        char32_t shaped = forms.isolated;
        if (joinsBefore && joinsAfter && forms.medial != 0) shaped = forms.medial;
        else if (joinsBefore && forms.final != 0) shaped = forms.final;
        else if (joinsAfter && forms.initial != 0) shaped = forms.initial;
        out += shaped != 0 ? shaped : c;
    }
    return out;
}

std::u32string VisualLine(const std::u32string& logical, const bool rightToLeft) {
    const std::u32string shaped = Shape(logical);
    // As it is: a left-to-right line in a left-to-right paragraph, and a line
    // with no letter at all in either - UI chrome ("[<]", "[>]", a lone
    // "[x] "), a number, a time - which a right-to-left paragraph would
    // otherwise turn round.
    bool rightToLeftLetter = false;
    bool letter = false;
    for (const char32_t c : shaped) {
        const Bidi type = BidiOf(c);
        rightToLeftLetter = rightToLeftLetter || type == Bidi::AL || type == Bidi::R;
        letter = letter || type == Bidi::AL || type == Bidi::R || type == Bidi::L;
    }
    if (!rightToLeftLetter && (!rightToLeft || !letter)) return shaped;
    // The options screen's switch marker "[x] " is UI chrome too: left, in
    // order, before the paragraph. (A space never joins, so the shaping of
    // what follows it is the same either way.)
    if (rightToLeft && shaped.size() > 4 && shaped[0] == '[' && shaped[2] == ']' && shaped[3] == ' ') {
        return shaped.substr(0, 4) + Reorder(shaped.substr(4), true);
    }
    return Reorder(shaped, rightToLeft);
}

std::u32string Visual(const std::u32string& logical, const bool rightToLeft) {
    std::u32string out;
    out.reserve(logical.size());
    std::size_t start = 0;
    for (std::size_t i = 0; i <= logical.size(); ++i) {
        if (i < logical.size() && logical[i] != '\n' && logical[i] != '\r') continue;
        out += VisualLine(logical.substr(start, i - start), rightToLeft);
        if (i < logical.size()) out += logical[i];
        start = i + 1;
    }
    return out;
}

} // namespace ArabicShaping

const std::u32string& VisualText::Of(const std::string& utf8, const bool rightToLeft) {
    auto& memo = m_memo[rightToLeft ? 1 : 0];
    if (const auto it = memo.find(utf8); it != memo.end()) return it->second;
    // Bounded: a clock makes a new string a second.
    if (memo.size() >= 4096) memo.clear();
    const std::u32string logical = Eth::Utf8ToCodePoints(utf8);
    bool plain = !rightToLeft;
    for (const char32_t c : logical) {
        if (!plain) break;
        plain = !ArabicShaping::IsArabic(c);
    }
    return memo.emplace(utf8, plain ? logical : ArabicShaping::Visual(logical, rightToLeft)).first->second;
}

} // namespace Penumbra::Render
