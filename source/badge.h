// "Completed" checkmark badge drawn straight into a Wii U iconTex.tga.
//
// Wii U icons are uncompressed 32-bit TGA files (128x128, bottom-left origin).
// The badge is composited in place, so the header, the footer and every pixel
// outside the top-right corner stay byte-identical.
#pragma once

#include <cstdint>
#include <vector>

namespace badge {

/// Geometry and colour shared by drawing and detection (same as the Mac script).
struct Geometry {
    int cx = 0, cy = 0;  ///< Circle centre in pixels, origin top-left.
    int radius = 0;
    int stroke = 0;      ///< Checkmark line width.
};

/// Badge geometry for an icon of the given width.
Geometry geometryFor(int width);

/// Decodes the icon to top-down RGBA.
///
/// Args:
///     file: Content of iconTex.tga.
///     rgba: Receives width * height * 4 bytes.
///     width: Receives the width.
///     height: Receives the height.
///
/// Returns:
///     False when the TGA is not an uncompressed 24/32-bit true-colour image.
bool decode(const std::vector<std::uint8_t>& file, std::vector<std::uint8_t>& rgba, int& width, int& height);

/// Draws the green checkmark badge in the top-right corner, in place.
///
/// Returns:
///     False (file untouched) when the TGA format is unsupported.
bool apply(std::vector<std::uint8_t>& file);

/// True when the badge colours are present where apply() paints them.
bool isMarked(const std::vector<std::uint8_t>& file);

}  // namespace badge
