#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace organizer {

// A single scanned file's metadata. This is what gets serialized to
// scan.json and handed off to the categorization service.
struct FileEntry {
    std::string path;            // absolute path, as scanned
    std::uintmax_t size_bytes;
    std::string extension;       // lowercase, includes leading '.', "" if none
    std::int64_t last_modified;  // unix epoch seconds
    std::string content_hash;    // sha-ish hash of file bytes, for change detection
    std::optional<std::string> content_sample; // first N bytes for text files, nullopt for binaries
};

// Recursively scans `root`, skipping symlinks and files above `max_size_bytes`
// (large binaries aren't useful to send to an LLM and slow the scan down).
// Throws std::filesystem::filesystem_error if `root` doesn't exist or isn't
// readable — callers should catch and report this rather than let it propagate
// to main, since a bad path is a user error, not a bug.
std::vector<FileEntry> scan_directory(
    const std::filesystem::path& root,
    std::uintmax_t max_size_bytes = 50 * 1024 * 1024,
    std::size_t content_sample_bytes = 4096);

// Writes entries to `output_path` as pretty-printed JSON.
void write_scan_json(
    const std::vector<FileEntry>& entries,
    const std::filesystem::path& output_path);

}  // namespace organizer
