#include "executor.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "hashing.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace organizer {

namespace {

ActionType parse_action(const std::string& s) {
    if (s == "move") return ActionType::Move;
    if (s == "rename") return ActionType::Rename;
    if (s == "skip") return ActionType::Skip;
    throw std::runtime_error("unknown action type in plan: " + s);
}

std::string timestamp_now() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    return buf;
}

}  // namespace

std::vector<PlannedAction> load_plan(const fs::path& plan_json) {
    std::ifstream file(plan_json);
    if (!file) {
        throw std::runtime_error("cannot open plan file: " + plan_json.string());
    }

    json j;
    file >> j;

    std::vector<PlannedAction> plan;
    for (const auto& entry : j) {
        PlannedAction pa;
        pa.original_path = entry.at("original_path").get<std::string>();
        pa.new_path = entry.value("new_path", "");
        pa.action = parse_action(entry.at("action").get<std::string>());
        pa.confidence = entry.value("confidence", 0.0);
        if (entry.contains("expected_hash") && !entry["expected_hash"].is_null()) {
            pa.expected_hash = entry["expected_hash"].get<std::string>();
        }
        plan.push_back(std::move(pa));
    }
    return plan;
}

std::vector<ValidationIssue> validate_plan(const std::vector<PlannedAction>& plan) {
    std::vector<ValidationIssue> issues;

    for (const auto& action : plan) {
        if (action.action == ActionType::Skip) continue;

        fs::path original(action.original_path);

        if (!fs::exists(original)) {
            issues.push_back({action.original_path, "original file no longer exists"});
            continue;
        }

        if (action.expected_hash) {
            auto current_hash = hash_file(original);
            if (current_hash != *action.expected_hash) {
                issues.push_back({action.original_path,
                                   "file changed since it was scanned (hash mismatch)"});
                continue;
            }
        }

        fs::path dest(action.new_path);
        if (fs::exists(dest)) {
            issues.push_back({action.new_path, "destination already exists (collision)"});
            continue;
        }
    }

    return issues;
}

void preview_plan(const std::vector<PlannedAction>& plan) {
    std::cout << "The following changes will be made:\n\n";

    int moves = 0, renames = 0, skips = 0;
    for (const auto& action : plan) {
        switch (action.action) {
            case ActionType::Move:
                std::cout << "  MOVE   " << action.original_path << " -> "
                          << action.new_path << "  (" << static_cast<int>(action.confidence * 100)
                          << "%)\n";
                ++moves;
                break;
            case ActionType::Rename:
                std::cout << "  RENAME " << action.original_path << " -> "
                          << action.new_path << "  (" << static_cast<int>(action.confidence * 100)
                          << "%)\n";
                ++renames;
                break;
            case ActionType::Skip:
                std::cout << "  SKIP   " << action.original_path << "\n";
                ++skips;
                break;
        }
    }

    std::cout << "\n  " << moves << " moves, " << renames << " renames, " << skips
              << " skipped\n"
              << "  No changes made yet.\n";
}

fs::path apply_plan(const std::vector<PlannedAction>& plan, const fs::path& journal_dir) {
    fs::create_directories(journal_dir);
    fs::path journal_path = journal_dir / ("journal_" + timestamp_now() + ".log");

    std::ofstream journal(journal_path);
    if (!journal) {
        throw std::runtime_error("cannot open journal file for writing: " + journal_path.string());
    }

    int applied = 0, errors = 0;

    for (const auto& action : plan) {
        if (action.action == ActionType::Skip) continue;

        fs::path src(action.original_path);
        fs::path dst(action.new_path);

        try {
            fs::create_directories(dst.parent_path());

            // Write to a temp path in the *destination* directory first,
            // then rename into place. fs::rename is atomic when source and
            // destination are on the same filesystem, which they are here
            // since temp_dst lives right next to the final dst. This avoids
            // ever leaving a half-written file at the final path.
            fs::path temp_dst = dst;
            temp_dst += ".organizer_tmp";
            fs::rename(src, temp_dst);
            fs::rename(temp_dst, dst);

            // Journal entry is written *after* the move succeeds and
            // flushed immediately, so a crash right after this line still
            // leaves an accurate record of everything done so far — never
            // a record of something that didn't actually happen.
            journal << action.original_path << "\t" << action.new_path << "\n";
            journal.flush();

            ++applied;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "  ERROR moving " << action.original_path << ": " << e.what() << "\n";
            ++errors;
        }
    }

    std::cout << "Applied " << applied << " changes";
    if (errors > 0) std::cout << ", " << errors << " errors";
    std::cout << ".\nJournal written to " << journal_path << "\n";

    return journal_path;
}

void undo_journal(const fs::path& journal_path) {
    std::ifstream journal(journal_path);
    if (!journal) {
        throw std::runtime_error("cannot open journal file: " + journal_path.string());
    }

    std::vector<JournalEntry> entries;
    std::string from, to;
    while (std::getline(journal, from, '\t') && std::getline(journal, to)) {
        entries.push_back({from, to});
    }

    // Reverse most-recent-first. Matters if a later action's destination
    // happened to be a path involved earlier in the run.
    int reverted = 0, errors = 0;
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        try {
            fs::path original(it->from);
            fs::create_directories(original.parent_path());
            fs::rename(it->to, it->from);
            ++reverted;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "  ERROR reverting " << it->to << " -> " << it->from << ": "
                      << e.what() << "\n";
            ++errors;
        }
    }

    std::cout << "Reverted " << reverted << " changes";
    if (errors > 0) std::cout << ", " << errors << " errors";
    std::cout << ".\n";
}

}  // namespace organizer
