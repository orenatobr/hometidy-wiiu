// Small POSIX file helpers shared by every module (work on the console and on the host).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fsx {

/// Reads a whole file as text/bytes. Returns false when it cannot be read.
bool readFile(const std::string& path, std::string& out);

/// Reads a whole file as bytes. Returns false when it cannot be read.
bool readBytes(const std::string& path, std::vector<std::uint8_t>& out);

/// Writes a file, replacing it. Returns false on any short write.
bool writeBytes(const std::string& path, const std::vector<std::uint8_t>& data);

/// Creates a directory and its parents. Returns true when it exists afterwards.
bool makeDirs(const std::string& path);

/// True when the path can be opened for reading.
bool exists(const std::string& path);

/// Names (not paths) inside a directory, without "." and "..", sorted.
std::vector<std::string> listDir(const std::string& path);

/// 32-bit FNV-1a hash, used to verify a written file by reading it back.
std::uint32_t fnv1a(const std::vector<std::uint8_t>& data);

}  // namespace fsx
