#include "sorter.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace sorter {
namespace {

// Folds the UTF-8 Latin-1 letters (U+00C0..U+00FF) to ASCII; '\0' means "drop".
char foldLatin1(unsigned cp) {
    static const char* kMap =
        "aaaaaaaceeeeiiii"   // C0..CF (U+00C6 AE and U+00C7 C handled approximately)
        "dnooooo\0ouuuuyts"  // D0..DF
        "aaaaaaaceeeeiiii"   // E0..EF
        "dnooooo\0ouuuuyty"; // F0..FF
    if (cp < 0xC0 || cp > 0xFF) return '\0';
    return kMap[cp - 0xC0];
}

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

// Compares strings treating digit runs as numbers ("mario kart 8" < "mario kart 10").
int naturalCompare(const std::string& a, const std::string& b) {
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (std::isdigit((unsigned char)a[i]) && std::isdigit((unsigned char)b[j])) {
            std::size_t ie = i, je = j;
            while (ie < a.size() && std::isdigit((unsigned char)a[ie])) ++ie;
            while (je < b.size() && std::isdigit((unsigned char)b[je])) ++je;
            const unsigned long long x = std::strtoull(a.substr(i, ie - i).c_str(), nullptr, 10);
            const unsigned long long y = std::strtoull(b.substr(j, je - j).c_str(), nullptr, 10);
            if (x != y) return x < y ? -1 : 1;
            i = ie;
            j = je;
        } else {
            if (a[i] != b[j]) return (unsigned char)a[i] < (unsigned char)b[j] ? -1 : 1;
            ++i;
            ++j;
        }
    }
    if (i < a.size()) return 1;
    if (j < b.size()) return -1;
    return 0;
}

}  // namespace

std::string normalize(const std::string& text) {
    std::string out;
    bool pendingSpace = false;
    auto emit = [&](char c) {
        if (pendingSpace && !out.empty()) out += ' ';
        pendingSpace = false;
        out += c;
    };
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char c = text[i];
        if (c < 0x80) {
            ++i;
            if (std::isalnum(c)) emit(char(std::tolower(c)));
            else if (c == '\'' ) continue;  // "Luigi's" -> "luigis"
            else pendingSpace = true;
        } else if (c == 0xC3 && i + 1 < text.size()) {
            const char f = foldLatin1(0xC0u + (static_cast<unsigned char>(text[i + 1]) - 0x80u));
            i += 2;
            if (f) emit(f);
        } else {
            // Any other multi-byte symbol (TM, R, curly quotes...) acts as a separator.
            ++i;
            while (i < text.size() && (text[i] & 0xC0) == 0x80) ++i;
            pendingSpace = true;
        }
    }
    for (const char* article : {"the ", "a ", "an "}) {
        const std::string a = article;
        if (out.compare(0, a.size(), a) == 0 && out.size() > a.size()) {
            out.erase(0, a.size());
            break;
        }
    }
    return out;
}

std::vector<Rule> parseRules(const std::string& text) {
    std::vector<Rule> rules;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        const auto p1 = line.find('|');
        if (p1 == std::string::npos) continue;
        const auto p2 = line.find('|', p1 + 1);
        Rule r;
        r.pattern = normalize(trim(line.substr(0, p1)));
        r.series = normalize(trim(line.substr(p1 + 1, p2 == std::string::npos ? std::string::npos : p2 - p1 - 1)));
        if (p2 != std::string::npos) r.year = std::atoi(trim(line.substr(p2 + 1)).c_str());
        if (!r.pattern.empty() && !r.series.empty()) rules.push_back(r);
    }
    return rules;
}

Key makeKey(const std::string& displayName, const std::vector<Rule>& rules) {
    Key k;
    k.name = normalize(displayName);
    k.series = k.name;
    for (const Rule& r : rules) {
        if (k.name.find(r.pattern) != std::string::npos) {
            k.series = r.series;
            k.year = r.year;
            break;
        }
    }
    return k;
}

bool less(const Key& a, const Key& b) {
    if (int c = naturalCompare(a.series, b.series)) return c < 0;
    if (a.year != b.year) return a.year < b.year;
    return naturalCompare(a.name, b.name) < 0;
}

}  // namespace sorter
