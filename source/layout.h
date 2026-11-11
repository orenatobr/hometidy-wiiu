// Parser and editor for the Wii U Menu "BaristaAccountSaveFile.dat" layout file.
//
// Format (reverse engineered by Yardape8000/menu_sort, confirmed on a real console):
//   - Home page: 300 NAND entries at 0x0, then 300 USB entries at 0x12C0.
//   - Folder N (1-based) at 0x2D24 + (N - 1) * 1976:
//       60 NAND entries, 60 USB entries, 56 bytes of info.
//       Info: folder name as UTF-16BE (17 chars) at +0, colour at +0x32.
//   - Entry (16 bytes, big endian): title hi, title lo, type, padding.
//   - A folder position i holds its title either in NAND[i] or USB[i]; the
//     other one stays empty (hi = lo = 0, type 1 for NAND and 2 for USB).
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace layout {

constexpr std::size_t kEntrySize = 16;
constexpr std::size_t kHomeSlots = 300;
constexpr std::size_t kHomeUsbBase = kHomeSlots * kEntrySize;  // 0x12C0
constexpr std::uint32_t kFolderType = 0x10;  ///< Entry type of a folder icon (id.lo = folder number).
constexpr std::size_t kFolderBase = 0x2D24;
constexpr std::size_t kFolderStride = 1976;
constexpr std::size_t kFolderSlots = 60;
constexpr std::size_t kInfoOffset = 2 * kFolderSlots * kEntrySize;
constexpr std::uint32_t kEmptyNandType = 1;
constexpr std::uint32_t kEmptyUsbType = 2;

/// Full 64-bit title id split like the file stores it.
struct TitleId {
    std::uint32_t hi = 0;
    std::uint32_t lo = 0;

    std::uint64_t value() const { return (static_cast<std::uint64_t>(hi) << 32) | lo; }
    bool operator==(const TitleId& o) const { return hi == o.hi && lo == o.lo; }
};

/// One occupied position of a folder.
struct Item {
    TitleId id;
    std::uint32_t type = 0;  ///< Raw entry type, preserved when the item moves.
    bool onUsb = false;      ///< True when stored in the USB array, false for NAND.
};

/// A folder with its occupied positions in display order.
struct Folder {
    int number = 0;            ///< 1-based folder number.
    std::string name;          ///< UTF-8 name, empty for unused folders.
    std::vector<Item> items;   ///< Occupied positions, ordered by slot index.
    bool ambiguous = false;    ///< A position has both NAND and USB filled: do not touch.
};

/// In-memory copy of the layout file with folder helpers.
class Layout {
public:
    /// Parses a layout file.
    ///
    /// Args:
    ///     bytes: Raw file content.
    ///
    /// Returns:
    ///     The layout, or nothing when the size does not match the format.
    static std::optional<Layout> parse(std::vector<std::uint8_t> bytes);

    /// Number of folder blocks stored in the file.
    int folderCount() const { return folderCount_; }

    /// Reads one folder.
    ///
    /// Args:
    ///     number: 1-based folder number, 1..folderCount().
    Folder folder(int number) const;

    /// Rewrites the occupied positions of a folder in the given order.
    ///
    /// Only the 120 entry slots of that folder change. Each title keeps its
    /// NAND/USB side and entry type.
    ///
    /// Args:
    ///     number: 1-based folder number.
    ///     order: Title ids; must be a permutation of the folder's titles.
    ///
    /// Returns:
    ///     False (and no change) when the folder is ambiguous or the order is
    ///     not a permutation of the current titles.
    bool setOrder(int number, const std::vector<TitleId>& order);

    /// Icons on the Home pages (apps, folders, loose games) in position order.
    ///
    /// Args:
    ///     ambiguous: Set to true when a position has both NAND and USB filled.
    std::vector<Item> homeItems(bool* ambiguous = nullptr) const;

    /// Rewrites the Home pages with the given icons, packed from position 0 with no gaps.
    ///
    /// Only the 600 Home entry slots change. Each icon keeps its NAND/USB side and type.
    ///
    /// Returns:
    ///     False (and no change) when the Home is ambiguous or `order` is not a
    ///     permutation of the current icons.
    bool setHomeOrder(const std::vector<Item>& order);

    const std::vector<std::uint8_t>& bytes() const { return data_; }

private:
    std::vector<std::uint8_t> data_;
    int folderCount_ = 0;
};

}  // namespace layout
