// The two jobs of the app (sort the Home folders, mark a game as completed),
// written against plain file paths so they run the same on the console and on the host.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "layout.h"
#include "sorter.h"
#include "titles.h"

namespace tasks {

/// Outcome of a write job. The UI turns it into a localized message.
enum class Status {
    Ok,
    ReadFailed,       ///< The file could not be read.
    Changed,          ///< The file changed since it was planned (console wrote it meanwhile).
    BackupFailed,
    WriteFailed,
    VerifyFailed,     ///< Read-back differs from what was written.
    NothingToDo,
    NoBackup,         ///< Unmark requested but no saved original exists.
    Unsupported,      ///< Icon is not an uncompressed TGA.
};

/// A Home layout file found on the console.
struct LayoutFile {
    std::string path;
    bool onSd = false;  ///< True for the Homebrew on Menu copy on the SD card.
};

/// Finds every BaristaAccountSaveFile.dat the Home menu may use.
///
/// Args:
///     sdRoot: Mount path of the SD card (e.g. "fs:/vol/external01").
///     mlcRoot: Mount path of the internal storage (e.g. "storage_mlc:").
///
/// Returns:
///     SD copies first (they win when the Homebrew on Menu plugin is active).
std::vector<LayoutFile> findLayoutFiles(const std::string& sdRoot, const std::string& mlcRoot);

/// One title row in a folder preview.
struct PlanRow {
    layout::TitleId id;
    std::string name;
    bool known = true;  ///< False when the title is not installed (shown by id, kept last).
    int from = 0;       ///< Position before, 0-based.
    int to = 0;         ///< Position after, 0-based.
    bool folder = false; ///< A folder icon on the Home (not an app or game).
};

/// Preview of one folder.
struct FolderPlan {
    int number = 0;              ///< 0 is the Home pages themselves, 1.. are folders.
    std::string name;
    std::vector<PlanRow> rows;   ///< In the new order.
    bool changed = false;
    bool skipped = false;        ///< Ambiguous folder: left untouched.
};

/// Result of planning a sort; nothing is written yet.
struct SortPlan {
    std::vector<std::uint8_t> original;
    std::vector<std::uint8_t> sorted;
    std::vector<FolderPlan> folders;  ///< Only folders that hold titles.
    int changedFolders = 0;
};

/// Computes the new order of every folder.
///
/// Args:
///     layoutBytes: Current content of the layout file.
///     catalog: Installed titles, for names.
///     rules: Series table.
///
/// Returns:
///     The plan, or nothing when the file is not a valid layout.
std::optional<SortPlan> planSort(const std::vector<std::uint8_t>& layoutBytes,
                                 const std::map<std::uint64_t, titles::Title>& catalog,
                                 const std::vector<sorter::Rule>& rules);

/// Result of a write job.
struct Result {
    Status status = Status::Ok;
    std::string backupPath;
    bool approximate = false;  ///< unmark without a saved original: pixels were rebuilt
};

/// Writes a planned sort: re-checks the file, saves a backup, writes, reads back.
///
/// Args:
///     path: Layout file path.
///     plan: Plan made from the content currently in `path`.
///     backupDir: Directory for the timestamped backup copy.
///     stamp: Text used in the backup name (e.g. "20261110_201500").
Result applySort(const std::string& path, const SortPlan& plan, const std::string& backupDir,
                 const std::string& stamp);

/// True when `path` currently holds exactly `expected` (used for the delayed re-check).
bool fileEquals(const std::string& path, const std::vector<std::uint8_t>& expected);

/// State of a title icon.
enum class IconState { Unreadable, Plain, Marked };

/// Reads `<dir>/meta/iconTex.tga` and tells whether it carries the badge.
IconState iconState(const titles::Title& title);

/// Draws the badge on the icon, saving the original first (once).
Result markIcon(const titles::Title& title, const std::string& backupDir);

/// Restores the original icon saved by markIcon(). Without a saved original (a mark made
/// by an older tool) the badge area is rebuilt from its surroundings and `approximate` is set.
Result unmarkIcon(const titles::Title& title, const std::string& backupDir);

}  // namespace tasks
