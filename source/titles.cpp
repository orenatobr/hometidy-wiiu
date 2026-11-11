#include "titles.h"

#include <dirent.h>

#include <cstdio>
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

std::string tagText(const std::string& xml, const std::string& tag) {
    const std::string open = "<" + tag + ">";
    const auto b = xml.find(open);
    if (b == std::string::npos) return "";
    const auto e = xml.find("</" + tag + ">", b);
    if (e == std::string::npos) return "";
    return collapse(decodeEntities(xml.substr(b + open.size(), e - b - open.size())));
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

}  // namespace titles
