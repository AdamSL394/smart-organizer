#include "hashing.h"

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace organizer {

std::string hash_file(const std::filesystem::path& path) {
    // FNV-1a, 64-bit. Streamed in chunks so we never load a whole large
    // file into memory just to hash it.
    constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;

    std::uint64_t hash = kOffsetBasis;

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "unreadable";
    }

    std::array<char, 65536> buffer{};
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        auto count = static_cast<std::size_t>(file.gcount());
        for (std::size_t i = 0; i < count; ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= kPrime;
        }
    }

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

}  // namespace organizer
