#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace organizer {

enum class ActionType { Move, Rename, Skip };

struct PlannedAction {
    std::string original_path;
    std::string new_path;         // empty for Skip
    ActionType action;
    double confidence;
    std::optional<std::string> expected_hash;  // hash at scan time, for staleness checks
};

struct ValidationIssue {
    std::string path;
    std::string reason;
};

// One completed move/rename, written to the journal so `undo` can reverse it.
struct JournalEntry {
    std::string from;
    std::string to;
};

std::vector<PlannedAction> load_plan(const std::filesystem::path& plan_json);

// Checks every action against the current filesystem state before anything
// is touched: does original_path still exist, does its hash still match
// what was scanned (nothing changed underneath us), and does new_path
// already collide with an existing file. Returns all issues found — an
// empty vector means the plan is safe to apply as-is.
std::vector<ValidationIssue> validate_plan(const std::vector<PlannedAction>& plan);

// Prints a human-readable preview of what apply() would do. Makes no
// filesystem changes.
void preview_plan(const std::vector<PlannedAction>& plan);

// Applies the plan: for each Move/Rename, writes to a temp path in the
// destination directory then does an atomic rename into place. Every
// successful step is appended to the journal file *before* moving to the
// next one, so a crash mid-run leaves a journal that accurately reflects
// what's already been done. Returns the path to the journal file written.
std::filesystem::path apply_plan(const std::vector<PlannedAction>& plan,
                                  const std::filesystem::path& journal_dir);

// Reads a journal file and reverses every entry in it, most-recent-first.
void undo_journal(const std::filesystem::path& journal_path);

}  // namespace organizer
