#include <filesystem>
#include <iostream>

#include "scanner.h"

namespace fs = std::filesystem;

namespace {

void print_usage() {
    std::cerr << "Usage:\n"
              << "  organizer scan <directory> [--out scan.json]\n"
              << "\n"
              << "  (categorize / preview / apply / undo are not implemented\n"
              << "   yet — this MVP covers the scan step end to end.)\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];

    if (command == "scan") {
        fs::path root = argv[2];
        fs::path out = "scan.json";
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--out" && i + 1 < argc) {
                out = argv[++i];
            }
        }

        try {
            std::cout << "Scanning " << root << "...\n";
            auto entries = organizer::scan_directory(root);

            std::uintmax_t total_bytes = 0;
            for (const auto& e : entries) total_bytes += e.size_bytes;

            organizer::write_scan_json(entries, out);

            std::cout << "Found " << entries.size() << " files ("
                      << (total_bytes / (1024 * 1024)) << " MB)\n"
                      << "Wrote " << out << " (" << entries.size() << " entries)\n";
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    std::cerr << "Unknown command: " << command << "\n";
    print_usage();
    return 1;
}
