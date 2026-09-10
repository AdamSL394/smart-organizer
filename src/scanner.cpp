#include "scanner.h"

#include <algorithm>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#include "hashing.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace organizer {

namespace {

std::string lowercase_extension(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return ext;
}

// Cheap heuristic: treat common text extensions as "safe to sample as text".
// Everything else gets no content_sample, only metadata — keeps binary
// garbage out of the LLM payload.
bool looks_like_text(const std::string& ext) {
    static const std::vector<std::string> text_exts = {
        ".txt", ".md", ".csv", ".json", ".log", ".yml", ".yaml",
        ".xml", ".html", ".htm", ".ini", ".cfg", ".conf"};
    return std::find(text_exts.begin(), text_exts.end(), ext) != text_exts.end();
}

std::optional<std::string> read_sample(const fs::path& path, std::size_t max_bytes) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;

    std::string buffer(max_bytes, '\0');
    file.read(buffer.data(), static_cast<std::streamsize>(max_bytes));
    buffer.resize(static_cast<std::size_t>(file.gcount()));
    return buffer;
}

}  // namespace

std::vector<FileEntry> scan_directory(const fs::path& root,
                                       std::uintmax_t max_size_bytes,
                                       std::size_t content_sample_bytes) {
    if (!fs::exists(root)) {
        throw fs::filesystem_error("path does not exist", root,
                                    std::make_error_code(std::errc::no_such_file_or_directory));
    }

    std::vector<FileEntry> entries;

    fs::recursive_directory_iterator it(
        root, fs::directory_options::skip_permission_denied);
    fs::recursive_directory_iterator end;

    for (; it != end; ++it) {
        const auto& entry = *it;

        // Skip symlinks explicitly rather than following them — we don't
        // want to scan (or later move) a file outside the intended tree.
        if (entry.is_symlink()) continue;
        if (!entry.is_regular_file()) continue;

        std::error_code ec;
        auto size = entry.file_size(ec);
        if (ec) continue;  // unreadable mid-scan (e.g. race with deletion); skip
        if (size > max_size_bytes) continue;

        auto mtime = entry.last_write_time(ec);
        if (ec) continue;
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            mtime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        auto epoch_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                 sctp.time_since_epoch())
                                 .count();

        FileEntry fe;
        fe.path = fs::absolute(entry.path()).string();
        fe.size_bytes = size;
        fe.extension = lowercase_extension(entry.path());
        fe.last_modified = epoch_seconds;
        fe.content_hash = hash_file(entry.path());
        fe.content_sample = looks_like_text(fe.extension)
                                 ? read_sample(entry.path(), content_sample_bytes)
                                 : std::nullopt;

        entries.push_back(std::move(fe));
    }

    return entries;
}

void write_scan_json(const std::vector<FileEntry>& entries, const fs::path& output_path) {
    json out = json::array();

    for (const auto& e : entries) {
        json j;
        j["path"] = e.path;
        j["size_bytes"] = e.size_bytes;
        j["extension"] = e.extension;
        j["last_modified"] = e.last_modified;
        j["content_hash"] = e.content_hash;
        if (e.content_sample) {
            j["content_sample"] = *e.content_sample;
        } else {
            j["content_sample"] = nullptr;
        }
        out.push_back(std::move(j));
    }

    std::ofstream file(output_path);
    // error_handler_t::replace swaps any invalid UTF-8 byte sequences for
    // U+FFFD instead of throwing. This matters here because content_sample
    // is a raw byte-count-based read (see read_sample) — it can truncate a
    // file right in the middle of a multi-byte UTF-8 character, or the file
    // may not be valid UTF-8 at all despite having a "text" extension.
    // Silently replacing a few bytes in a preview sample is harmless; an
    // uncaught exception aborting the whole scan over one bad file is not.
    file << out.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
}

}  // namespace organizer
