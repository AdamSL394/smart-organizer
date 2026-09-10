# smart-organizer

A local file organizer: a C++ scanner walks a directory and extracts
metadata, an LLM categorizes the files (Python service),
and a C++ executor applies the resulting changes safely — with a
dry-run preview, atomic moves, a journal of every change, and a full
undo.

The interesting engineering is on the C++ side: this isn't just a
script that shuffles files around. Every apply is validated first,
every move is atomic, every change is journaled before moving on to
the next, and the whole run can be reversed with one command.

## Status

- Scanner — recursively walks a directory, hashes and samples file
  content, writes `scan.json`
- Executor — validates, previews, atomically applies, and undoes a
  plan
- Categorizer — Python service that turns `scan.json` into
  `plan.json` using an LLM

## Build

Requires CMake 3.16+ and a C++20 compiler. Dependencies
([nlohmann/json](https://github.com/nlohmann/json)) are fetched
automatically by CMake.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

## Usage

```sh
# 1. Scan a directory
./build/organizer scan ~/Downloads --out scan.json

# 2. (Not yet implemented) Hand scan.json to the categorization
#    service to produce plan.json

# 3. Preview the plan — validates against the current filesystem
#    state and shows exactly what would change. No files are touched.
./build/organizer preview plan.json

# 4. Apply — refuses to run if validation finds any issue. Otherwise
#    applies every change atomically and writes a journal.
./build/organizer apply plan.json --journal-dir .organizer

# 5. Undo — reverses every entry in a journal, most-recent-first.
./build/organizer undo .organizer/journal_20260910_020045.log
```

### Plan format

`plan.json` is a list of actions:

```json
[
  {
    "original_path": "/Users/you/Downloads/IMG_4821.jpg",
    "new_path": "/Users/you/Photos/2024/Vacation/IMG_4821.jpg",
    "action": "move",
    "confidence": 0.94,
    "expected_hash": "301aab7e31352870"
  }
]
```

`expected_hash` is optional — if present, `validate_plan` confirms
the file hasn't changed since it was scanned before allowing the
move.

## Design notes

- **Atomic moves**: each apply writes to `<dest>.organizer_tmp` next
  to the final destination, then renames into place. Both renames are
  atomic on the same filesystem, so a crash mid-run never leaves a
  half-written file at the final path.

- **Journal-before-next**: each journal line is written and flushed
  immediately after its move succeeds — the journal is always
  accurate to what's actually happened on disk, even if the process
  is killed mid-run.

- **Validation before any writes**: `apply` checks every action
  up front (source still exists, hash still matches, destination
  doesn't already exist) and refuses to touch the filesystem at all
  if anything's wrong.

- **Known limitation**: atomicity relies on `rename()`, which
  requires source and destination to be on the same filesystem.
  Cross-filesystem moves aren't handled yet — see the note in
  `executor.cpp`.

## License
...
