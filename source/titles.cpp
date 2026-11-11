#include "titles.h"

#include <dirent.h>

#include <cstdio>
#include <cctype>
#include <cstdlib>

#include "fsx.h"

namespace titles {
namespace {

std::string decodeEntities(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') {
            out += s[i];
            continue;
        }
        const auto semi = s.find(';', i);
        if (semi == std::string::npos || semi - i > 8) {
            out += s[i];
            continue;
        }
        const std::string ent = s.substr(i + 1, semi - i - 1);
        if (ent == "amp") out += '&';
        else if (ent == "lt") out += '<';
        else if (ent == "gt") out += '>';
        else if (ent == "quot") out += '"';
        else if (ent == "apos") out += '\'';
        else if (!ent.empty() && ent[0] == '#') {
            const long cp = ent[1] == 'x' ? std::strtol(ent.c_str() + 2, nullptr, 16)
                                          : std::strtol(ent.c_str() + 1, nullptr, 10);
            if (cp > 0 && cp < 0x80) out += char(cp);
            else if (cp >= 0x80 && cp < 0x800) {
                out += char(0xC0 | (cp >> 6));
                out += char(0x80 | (cp & 0x3F));
            } else if (cp >= 0x800 && cp < 0x10000) {
                out += char(0xE0 | (cp >> 12));
                out += char(0x80 | ((cp >> 6) & 0x3F));
                out += char(0x80 | (cp & 0x3F));
            }
        } else {
            out += s.substr(i, semi - i + 1);
        }
        i = semi;
    }
    return out;
}

std::string collapse(const std::string& s) {
    std::string out;
    bool space = false;
    for (char c : s) {
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
            space = true;
            continue;
        }
        if (space && !out.empty()) out += ' ';
        space = false;
        out += c;
    }
    return out;
}

// Text of <tag ...>text</tag>. Real meta.xml files put attributes on every tag
// (`<longname_en type="string" length="512">`), and may start with a BOM.
std::string tagText(const std::string& xml, const std::string& tag) {
    const std::string open = "<" + tag;
    for (std::size_t b = xml.find(open); b != std::string::npos; b = xml.find(open, b + 1)) {
        const std::size_t after = b + open.size();
        if (after >= xml.size()) return "";
        const char next = xml[after];
        if (next != '>' && next != ' ' && next != '\t' && next != '\r' && next != '\n' && next != '/') continue;  // longer tag name
        const std::size_t gt = xml.find('>', after);
        if (gt == std::string::npos) return "";
        if (xml[gt - 1] == '/') return "";  // <tag/>: empty
        const std::size_t end = xml.find("</" + tag + ">", gt);
        if (end == std::string::npos) return "";
        return collapse(decodeEntities(xml.substr(gt + 1, end - gt - 1)));
    }
    return "";
}

}  // namespace

std::string nameFromMeta(const std::string& xml) {
    for (const char* lang : {"en", "pt", "es", "fr", "de", "it", "nl", "ru", "ja"}) {
        const std::string n = tagText(xml, std::string("longname_") + lang);
        if (!n.empty()) return n;
    }
    return "";
}

std::map<std::uint64_t, Title> scan(const std::vector<std::string>& roots) {
    std::map<std::uint64_t, Title> out;
    for (const std::string& root : roots) {
        for (const char* kind : {"00050000", "00050002"}) {
            const std::string typeDir = root + "/" + kind;
            for (const std::string& entry : fsx::listDir(typeDir)) {
                if (entry.size() != 8) continue;
                const std::uint64_t id = (std::strtoull(kind, nullptr, 16) << 32) | std::strtoull(entry.c_str(), nullptr, 16);
                if (out.count(id)) continue;
                const std::string dir = typeDir + "/" + entry;
                std::string xml;
                if (!fsx::readFile(dir + "/meta/meta.xml", xml)) continue;
                const std::string name = nameFromMeta(xml);
                if (name.empty()) continue;
                out[id] = {id, name, dir};
            }
        }
    }
    return out;
}

std::map<std::uint64_t, Title> scanSystemApps(const std::string& mlcRoot) {
    std::map<std::uint64_t, Title> out;
    if (mlcRoot.empty()) return out;
    const std::string typeDir = mlcRoot + "/sys/title/00050010";
    for (const std::string& entry : fsx::listDir(typeDir)) {
        if (entry.size() != 8) continue;
        const std::uint64_t id = (std::uint64_t(0x00050010) << 32) | std::strtoull(entry.c_str(), nullptr, 16);
        std::string xml;
        if (!fsx::readFile(typeDir + "/" + entry + "/meta/meta.xml", xml)) continue;
        const std::string name = nameFromMeta(xml);
        if (!name.empty()) out[id] = {id, name, typeDir + "/" + entry};
    }
    return out;
}

std::uint64_t homebrewId(const std::string& relativePath) {
    std::uint32_t h = 0;  // same polynomial hash as the plugin: h = 37 * h + c
    for (unsigned char c : relativePath) h = 37 * h + c;
    return (std::uint64_t(0x0005000F) << 32) | h;
}

std::string nameFromWuhbMeta(const std::vector<std::uint8_t>& tail) {
    const std::string text(tail.begin(), tail.end());
    const std::string key = "longname=";
    const auto b = text.find(key);
    if (b == std::string::npos) return "";
    const auto e = text.find_first_of("\r\n", b);
    return collapse(text.substr(b + key.size(), e == std::string::npos ? std::string::npos : e - b - key.size()));
}

std::map<std::uint64_t, Title> scanHomebrew(const std::string& sdRoot) {
    std::map<std::uint64_t, Title> out;
    const std::string base = sdRoot + "/wiiu/apps";
    auto isApp = [](const std::string& n) {
        const auto dot = n.rfind('.');
        if (dot == std::string::npos) return false;
        std::string ext = n.substr(dot);
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return ext == ".wuhb" || ext == ".rpx";
    };
    auto add = [&](const std::string& rel) {
        std::vector<std::uint8_t> tail;
        std::string name;
        if (rel.size() > 5 && rel.compare(rel.size() - 5, 5, ".wuhb") == 0 &&
            fsx::readTail(sdRoot + "/" + rel, 128 * 1024, tail)) {
            name = nameFromWuhbMeta(tail);
        }
        if (name.empty()) {  // file name without extension
            const auto slash = rel.rfind('/');
            name = rel.substr(slash + 1, rel.rfind('.') - slash - 1);
        }
        const std::uint64_t id = homebrewId(rel);
        out[id] = {id, name, sdRoot + "/" + rel};
    };
    for (const std::string& a : fsx::listDir(base)) {
        if (isApp(a)) {
            add("wiiu/apps/" + a);
            continue;
        }
        for (const std::string& b : fsx::listDir(base + "/" + a)) {
            if (isApp(b)) {
                add("wiiu/apps/" + a + "/" + b);
                continue;
            }
            for (const std::string& c : fsx::listDir(base + "/" + a + "/" + b)) {
                if (isApp(c)) add("wiiu/apps/" + a + "/" + b + "/" + c);
            }
        }
    }
    return out;
}

}  // namespace titles
