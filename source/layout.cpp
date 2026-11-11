#include "layout.h"

#include <algorithm>
#include <utility>

namespace layout {
namespace {

std::uint32_t be32(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | p[3];
}

void putBe32(std::uint8_t* p, std::uint32_t v) {
    p[0] = std::uint8_t(v >> 24);
    p[1] = std::uint8_t(v >> 16);
    p[2] = std::uint8_t(v >> 8);
    p[3] = std::uint8_t(v);
}

std::size_t folderOffset(int number) { return kFolderBase + std::size_t(number - 1) * kFolderStride; }

bool isEmpty(const std::uint8_t* e) { return be32(e) == 0 && be32(e + 4) == 0; }

void writeEmpty(std::uint8_t* e, std::uint32_t type) {
    putBe32(e, 0);
    putBe32(e + 4, 0);
    putBe32(e + 8, type);
    putBe32(e + 12, 0);
}

void appendUtf8(std::string& out, char32_t c) {
    if (c < 0x80) {
        out += char(c);
    } else if (c < 0x800) {
        out += char(0xC0 | (c >> 6));
        out += char(0x80 | (c & 0x3F));
    } else {
        out += char(0xE0 | (c >> 12));
        out += char(0x80 | ((c >> 6) & 0x3F));
        out += char(0x80 | (c & 0x3F));
    }
}

}  // namespace

std::optional<Layout> Layout::parse(std::vector<std::uint8_t> bytes) {
    if (bytes.size() < kFolderBase + kFolderStride) return std::nullopt;
    const std::size_t rest = bytes.size() - kFolderBase;
    if (rest % kFolderStride != 0) return std::nullopt;
    Layout l;
    l.data_ = std::move(bytes);
    l.folderCount_ = int(rest / kFolderStride);
    return l;
}

Folder Layout::folder(int number) const {
    Folder f;
    f.number = number;
    const std::uint8_t* base = data_.data() + folderOffset(number);
    const std::uint8_t* nand = base;
    const std::uint8_t* usb = base + kFolderSlots * kEntrySize;

    const std::uint8_t* info = base + kInfoOffset;
    for (int i = 0; i < 17; ++i) {
        const char32_t c = (char32_t(info[i * 2]) << 8) | info[i * 2 + 1];
        if (c == 0) break;
        appendUtf8(f.name, c);
    }

    for (std::size_t i = 0; i < kFolderSlots; ++i) {
        const std::uint8_t* n = nand + i * kEntrySize;
        const std::uint8_t* u = usb + i * kEntrySize;
        const bool hasN = !isEmpty(n);
        const bool hasU = !isEmpty(u);
        if (hasN && hasU) f.ambiguous = true;
        if (!hasN && !hasU) continue;
        const std::uint8_t* e = hasN ? n : u;
        f.items.push_back({{be32(e), be32(e + 4)}, be32(e + 8), !hasN});
    }
    return f;
}

bool Layout::setOrder(int number, const std::vector<TitleId>& order) {
    if (number < 1 || number > folderCount_) return false;
    const Folder current = folder(number);
    if (current.ambiguous || order.size() != current.items.size()) return false;

    // Match every requested id to one current item (duplicates are consumed once).
    std::vector<Item> pool = current.items;
    std::vector<Item> placed;
    for (const TitleId& id : order) {
        auto it = std::find_if(pool.begin(), pool.end(), [&](const Item& x) { return x.id == id; });
        if (it == pool.end()) return false;
        placed.push_back(*it);
        pool.erase(it);
    }

    std::uint8_t* base = data_.data() + folderOffset(number);
    std::uint8_t* nand = base;
    std::uint8_t* usb = base + kFolderSlots * kEntrySize;
    for (std::size_t i = 0; i < kFolderSlots; ++i) {
        std::uint8_t* n = nand + i * kEntrySize;
        std::uint8_t* u = usb + i * kEntrySize;
        writeEmpty(n, kEmptyNandType);
        writeEmpty(u, kEmptyUsbType);
        if (i >= placed.size()) continue;
        const Item& it = placed[i];
        std::uint8_t* e = it.onUsb ? u : n;
        putBe32(e, it.id.hi);
        putBe32(e + 4, it.id.lo);
        putBe32(e + 8, it.type);
        putBe32(e + 12, 0);
    }
    return true;
}

}  // namespace layout
