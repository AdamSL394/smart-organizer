#include <filesystem>
#include <iostream>

#include "executor.h"
#include "scanner.h"

namespace fs = std::filesystem;

namespace {

void print_usage() {
    std::cerr << "Usage:\n"
              << "  organizer scan <directory> [--out scan.json]\n"
              << "  organizer preview <plan.json>\n"
              << "  organizer apply <plan.json> [--journal-dir .organizer]\n"
              << "  organizer undo <journal.log>\n"
              << "\n"
              << "  (categorize is not implemented yet — plan.json is\n"
              << "   currently hand-written or produced by an external step.)\n";
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

    if (command == "preview") {
        try {
            auto plan = organizer::load_plan(argv[2]);
            auto issues = organizer::validate_plan(plan);
            if (!issues.empty()) {
                std::cout << "Validation found " << issues.size() << " issue(s):\n";
                for (const auto& issue : issues) {
                    std::cout << "  " << issue.path << ": " << issue.reason << "\n";
                }
                std::cout << "\n";
            }
            organizer::preview_plan(plan);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    if (command == "apply") {
        fs::path journal_dir = ".organizer";
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--journal-dir" && i + 1 < argc) {
                journal_dir = argv[++i];
            }
        }
        try {
            auto plan = organizer::load_plan(argv[2]);
            auto issues = organizer::validate_plan(plan);
            if (!issues.empty()) {
                std::cerr << "Refusing to apply: " << issues.size()
                          << " validation issue(s) found. Run 'preview' first.\n";
                for (const auto& issue : issues) {
                    std::cerr << "  " << issue.path << ": " << issue.reason << "\n";
                }
                return 1;
            }
            auto journal = organizer::apply_plan(plan, journal_dir);
            std::cout << "Run './organizer undo " << journal.string() << "' to revert.\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    if (command == "undo") {
        try {
            organizer::undo_journal(argv[2]);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    std::cerr << "Unknown command: " << command << "\n";
    print_usage();
    return 1;
}
