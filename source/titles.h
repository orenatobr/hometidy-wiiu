// Installed title catalog read from the title directories of the console.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace titles {

/// One installed title.
struct Title {
    std::uint64_t id = 0;   ///< Full title id, e.g. 0x0005000010143600.
    std::string name;       ///< Display name from meta.xml (UTF-8, single line).
    std::string dir;        ///< Title directory; meta/ lives inside it.
};

/// Extracts the display name from a meta.xml document.
///
/// Prefers the English long name, then the other languages in a fixed order.
///
/// Args:
///     xml: Content of meta/meta.xml.
///
/// Returns:
///     Name with entities decoded and line breaks turned into spaces, or an
///     empty string when no long name exists.
std::string nameFromMeta(const std::string& xml);

/// Scans `<root>/<00050000|00050002>/<lo8hex>/meta/meta.xml` for each root.
///
/// Args:
///     roots: Paths such as "storage_usb:/usr/title".
///
/// Returns:
///     Titles by id; a title found in several roots keeps the first one.
std::map<std::uint64_t, Title> scan(const std::vector<std::string>& roots);

}  // namespace titles
