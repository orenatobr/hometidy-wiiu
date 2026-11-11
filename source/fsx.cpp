#include "fsx.h"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdio>

namespace fsx {

bool readBytes(const std::string& path, std::vector<std::uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    std::uint8_t buf[8192];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    const bool ok = !std::ferror(f);
    std::fclose(f);
    return ok;
}

bool readFile(const std::string& path, std::string& out) {
    std::vector<std::uint8_t> b;
    if (!readBytes(path, b)) return false;
    out.assign(b.begin(), b.end());
    return true;
}

bool writeBytes(const std::string& path, const std::vector<std::uint8_t>& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    return (std::fclose(f) == 0) && ok;
}

bool exists(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f) {
        std::fclose(f);
        return true;
    }
    DIR* d = opendir(path.c_str());
    if (d) {
        closedir(d);
        return true;
    }
    return false;
}

bool makeDirs(const std::string& path) {
    for (std::size_t i = 1; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            const std::string part = path.substr(0, i);
            if (part.back() == ':' || part.back() == '/') continue;
            mkdir(part.c_str(), 0777);  // existing directories simply fail with EEXIST
        }
    }
    DIR* d = opendir(path.c_str());
    if (!d) return false;
    closedir(d);
    return true;
}

std::vector<std::string> listDir(const std::string& path) {
    std::vector<std::string> names;
    DIR* d = opendir(path.c_str());
    if (!d) return names;
    while (dirent* e = readdir(d)) {
        const std::string n = e->d_name;
        if (n != "." && n != "..") names.push_back(n);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    return names;
}

std::uint32_t fnv1a(const std::vector<std::uint8_t>& data) {
    std::uint32_t h = 2166136261u;
    for (std::uint8_t b : data) {
        h ^= b;
        h *= 16777619u;
    }
    return h;
}

}  // namespace fsx
