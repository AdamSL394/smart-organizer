#pragma once

#include <filesystem>
#include <string>

namespace organizer {

// Returns a hex-encoded FNV-1a hash of the file's contents.
// Not cryptographic — this is for detecting "did this file change between
// scan and apply", not for security. Swap in SHA-256 (e.g. OpenSSL) later
// if you need collision resistance for a real dedup feature.
std::string hash_file(const std::filesystem::path& path);

}  // namespace organizer
